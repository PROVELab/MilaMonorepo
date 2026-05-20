import java.util.Map;
import java.util.Optional;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.List;
import java.util.concurrent.ConcurrentHashMap;

import javax.swing.Timer;
import javax.swing.SwingUtilities;

//Parses Can messages and updates display. Also formats user messages to Can before sending to telem
public class CanParser {

    private final TelemetryLookup lookup;
    private final NotificationPanel notifications;
    private final MainPanel mainPanel;
    private final MainFrame mainFrame;
    private SerialBridge sb;
    private final GeneratedPacketVisitor packetCallbacks;
    private final GeneratedCANFrameVisitor canFrameCallbacks;
    private final Map<TelemetryLookup.FrameKey, Timer> timeoutTimers = new ConcurrentHashMap<>();

    public CanParser(TelemetryLookup lookup, NotificationPanel notifications, MainPanel mainPanel, MainFrame mainFrame) {
        this.lookup = lookup;
        this.notifications = notifications;
        this.mainPanel=mainPanel;
        this.mainFrame = mainFrame;
        this.packetCallbacks = new GeneratedPacketVisitor(lookup, notifications, mainPanel);
        this.canFrameCallbacks = new GeneratedCANFrameVisitor(lookup, notifications, mainPanel);

        System.out.println("Can init");
        final String portName = "/dev/ttyACM0"; final int baud = 115200;

        // Pass the status update callback to the SerialBridge
        this.sb = new SerialBridge(portName, baud, this::onMessageRecv, this::logInvalidFrame, this::updateSerialStatus);

        // Initial connection attempt
        // Run in a new thread to avoid blocking the UI during initial sleep in SerialBridge
        new Thread(() -> {
            if (!this.sb.connect()) {
                TelemetryUpdate("Initial SerialBridge connection failed.", NotificationPanel.Status.CRITICAL);
            }
        }).start();

        mainFrame.setCanParser(this);

        //Take user commands
        notifications.setOnCommandSubmit(cmd -> {
            buildPayloadFromCommand(cmd).ifPresent(payload -> {
                try {
                    if (sb.isConnected()) {
                        sb.sendMessage(payload);
                    } else {
                        TelemetryUpdate("Cannot send command: Serial port not connected.", NotificationPanel.Status.CRITICAL);
                    }
                } catch (IOException e) {
                    TelemetryUpdate("Failed to send command: " + e.getMessage(), NotificationPanel.Status.CRITICAL);
                }
            });
        });
    }

    // New method to handle status updates from SerialBridge
    private void updateSerialStatus(boolean connected) {
        SwingUtilities.invokeLater(() -> mainFrame.setSerialStatus(connected));
        if (connected) {
            TelemetryUpdate("Serial port connected.", NotificationPanel.Status.OK);
        } else {
            // This message can be noisy if it keeps trying and failing, but it's useful.
            TelemetryUpdate("Serial port disconnected.", NotificationPanel.Status.CRITICAL);
        }
    }

    // New public method for the UI to call
    public void restartSerialConnection() {
        TelemetryUpdate("Attempting to restart serial connection...", NotificationPanel.Status.OK);
        // Run in a new thread to avoid blocking the UI
        new Thread(() -> {
            sb.close();
            try {
                // Brief pause before trying to reconnect
                Thread.sleep(500);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
            if (!sb.connect()) {
                TelemetryUpdate("SerialBridge restart failed.", NotificationPanel.Status.CRITICAL);
            }
        }).start();
    }

    private Optional<byte[]> buildPayloadFromCommand(String input) {
        if (input == null) return Optional.empty();
        input = input.trim();

        if (input.startsWith("updateValue")) {
            String numStr = input.substring("updateValue".length()).trim();
            Optional<Integer> parsed = parseInt32(numStr);
            if (parsed.isEmpty()) return Optional.empty();

            int value = parsed.get();
            byte[] msg = new byte[8];
            // Pack the integer into the first 4 bytes (little-endian)
            msg[0] = (byte) (value);
            msg[1] = (byte) (value >> 8);
            msg[2] = (byte) (value >> 16);
            msg[3] = (byte) (value >> 24);

            return Optional.of(msg);
        }
        TelemetryUpdate("unable to interpret provided user command", NotificationPanel.Status.WARNING);
        return Optional.empty();
    }

    private static Optional<Integer> parseInt32(String s) {
        if (s == null || s.isEmpty()) return Optional.empty();
        try { return Optional.of(Integer.parseInt(s.replace("_",""))); // signed decimal
        } catch (NumberFormatException e) {
            return Optional.empty();
        }
    }

    private void logInvalidFrame(byte[] payload) {
        java.nio.ByteBuffer bb = java.nio.ByteBuffer.wrap(payload).order(java.nio.ByteOrder.LITTLE_ENDIAN);
        int id   = bb.getInt();
        long data = bb.getLong();
        final int CanIdMask        = 0b1111111;
        final int functionCodeMask = 0b1111 << 7;
        final int extendedIdMask   = 0x3FFFF << 11;
        final int nodeId       = (id & CanIdMask);
        final int functionCode = (id & functionCodeMask) >> 7;
        final int extendedId   = (id & extendedIdMask)   >> 11;
        System.out.println(String.format(
            " Invalid CAN frame: id=0x%08X func=0x%08X ext=0x%08X data=0x%016X",
            nodeId, functionCode, extendedId, data));
    }    

    private void onMessageRecv(byte[] loraPayload) {
        if (loraPayload == null || loraPayload.length == 0) {
            TelemetryUpdate("onMessageRecv got empty payload!", NotificationPanel.Status.WARNING);
            return;
        }

        List<TelemetryParser.ParsedPacket> packets = TelemetryParser.parse(loraPayload, this.lookup);
        for (TelemetryParser.ParsedPacket packet : packets) {
            if (packet instanceof TelemetryParser.CANDataFramePacket) {
                processCANDataFrame((TelemetryParser.CANDataFramePacket) packet);
            } else {
                processPlottablePacket(packet);
            }
            packet.accept(this.packetCallbacks);
        }
    }

    private void processCANDataFrame(TelemetryParser.CANDataFramePacket p) {
        BitStream payloadStream = new BitStream(ByteBuffer.wrap(p.payload).order(ByteOrder.LITTLE_ENDIAN));
        int frameIndex = (int) payloadStream.read(Constants.maxFrameCntBits);
        int nodeId = p.nodeID();
        
        processAndPlotCANData(nodeId, frameIndex, payloadStream);
    }

    private void processPlottablePacket(TelemetryParser.ParsedPacket packet) {
        int nodeId = Constants.specialIDs.vitalsID;
        int frameIndex = packet.packetIndex;

        var frameOpt = lookup.getFrame(nodeId, frameIndex);
        if (frameOpt.isEmpty()) {
            TelemetryUpdate("processPlottablePacket from unknown vitalsID/frameIndex: " + nodeId + "/" + frameIndex,
                    NotificationPanel.Status.WARNING);
            return;
        }
        TelemetryRecords.CANFrame frame = frameOpt.get();

        if (frame.dataTimeout() > 0) {
            onFrameReceivedResetTimer(nodeId, frameIndex, frame.dataTimeout());
        }

        int[] values = packet.getValues();
        if (values.length != frame.numData()) {
            TelemetryUpdate("Mismatched field count for " + packet.packetName, NotificationPanel.Status.CRITICAL);
            return;
        }

        for (int i = 0; i < frame.numData(); i++) {
            TelemetryLookup.DataKey dataKey = new TelemetryLookup.DataKey(nodeId, frameIndex, i);
            var dataInfoOpt = lookup.getDataInfo(dataKey);
            if (dataInfoOpt.isEmpty()) {
                TelemetryUpdate("Missing DataInfo for plottable packet. Node: " + nodeId + " Frame: " + frameIndex + " DataIndex: " + i,
                        NotificationPanel.Status.WARNING);
                continue;
            }
            TelemetryRecords.DataInfo dataInfo = dataInfoOpt.get();
            
            int dataValue = values[i];

            checkDataValue(dataKey, dataInfo, dataValue);
            if (!mainPanel.addDataPoint(nodeId, frameIndex, i, dataValue)) {
                TelemetryUpdate("Failed to add data point to main panel. Node: " + nodeId + " Frame: " + frameIndex
                        + " DataIndex: " + i + " Value: " + dataValue, NotificationPanel.Status.WARNING);
            }
        }
    }

    private void processAndPlotCANData(int nodeId, int frameIndex, BitStream dataStream) {
        var frameOpt = lookup.getFrame(nodeId, frameIndex);
        if (frameOpt.isEmpty()) {
            TelemetryUpdate("Transmit Data from unknown nodeId/frameIndex: " + nodeId + "/" + frameIndex,
                    NotificationPanel.Status.WARNING);
            return;
        }
        TelemetryRecords.CANFrame frame = frameOpt.get();

        if (frame.dataTimeout() > 0) {
            onFrameReceivedResetTimer(nodeId, frameIndex, frame.dataTimeout());
        }

        int[] parsedValues = new int[frame.numData()];

        for (int i = 0; i < frame.numData(); i++) {
            TelemetryLookup.DataKey dataKey = new TelemetryLookup.DataKey(nodeId, frameIndex, i);
            var dataInfoOpt = lookup.getDataInfo(dataKey);
            if (dataInfoOpt.isEmpty()) {
                TelemetryUpdate("Missing DataInfo for a data indicated to exist by frame's numData value."
                        + " Node: " + nodeId + " Frame: " + frameIndex + " DataIndex: " + i,
                        NotificationPanel.Status.WARNING);
                return;
            }
            TelemetryRecords.DataInfo dataInfo = dataInfoOpt.get();

            if (dataInfo.bitLength() < 0 || dataInfo.bitLength() > 32) {
                TelemetryUpdate("DataInfo has invalid bitLength. Node: " + nodeId + " Frame: " + frameIndex
                        + " DataIndex: " + i + " bitLength: " + dataInfo.bitLength(),
                        NotificationPanel.Status.WARNING);
                return;
            }

            if (!dataStream.hasRemaining()) {
                 TelemetryUpdate("Data stream ended prematurely. Node: " + nodeId + " Frame: " + frameIndex,
                        NotificationPanel.Status.WARNING);
                return;
            }

            long rawValue = dataStream.read(dataInfo.bitLength());
            if (dataInfo.min() < 0 && (dataInfo.bitLength() < 64) && (rawValue & (1L << (dataInfo.bitLength() - 1))) != 0) {
                rawValue |= -1L << dataInfo.bitLength();
            }
            int dataValue = (int) rawValue + dataInfo.min();

            parsedValues[i] = dataValue;

            checkDataValue(dataKey, dataInfo, dataValue);
            if (!mainPanel.addDataPoint(nodeId, frameIndex, i, dataValue)) {
                TelemetryUpdate("Failed to add data point to main panel. Node: " + nodeId + " Frame: " + frameIndex
                        + " DataIndex: " + i + " Value: " + dataValue, NotificationPanel.Status.WARNING);
            }
        }

        if (frame.enableTelemCallback() == true) {
            CANFrameParser.ParsedCANFrame packet = CANFrameParser.createPacket(nodeId, frameIndex, parsedValues);
            if (packet != null) {
                packet.accept(this.canFrameCallbacks);
            }
        }
    }

    private void onFrameReceivedResetTimer(int nodeId, int frameIndex, int timeoutMs) {
        TelemetryLookup.FrameKey key = new TelemetryLookup.FrameKey(nodeId, frameIndex);
        Timer timer = timeoutTimers.computeIfAbsent(key, k -> {
            Timer t = new Timer(timeoutMs, e -> onFrameTimeout(k));
            t.setRepeats(false);
            return t;
        });
        timer.restart();
    }

    private void onFrameTimeout(TelemetryLookup.FrameKey key) {
        // Get the expected timeout from the lookup table to make the message more informative.
        int expectedTimeout = lookup.getFrame(key.nodeId(), key.frameIndex())
                                .map(TelemetryRecords.CANFrame::dataTimeout)
                                .orElse(0);

        String nodeName = lookup.getNodeName(key.nodeId()).orElse("ID " + key.nodeId());

        // Construct a message similar to the old one. The "overdue by" amount was an
        // artifact of the old polling mechanism and isn't directly applicable here,
        // as the timer fires precisely when the timeout occurs.
        String msg = String.format("Missing frame: %s (frameIndex=%d). Expected every ~%dms.",
                                   nodeName,
                                   key.frameIndex(),
                                   expectedTimeout);

        TelemetryUpdate(msg, NotificationPanel.Status.WARNING);
    }
    
    private void checkDataValue(TelemetryLookup.DataKey key, TelemetryRecords.DataInfo info, int value) {
        // TODO: Implement user's checkDataValue logic if it exists.
        // This is a placeholder.
        if (value < info.minCritical() || value > info.maxCritical()) {
            // ... post critical warning
        } else if (value < info.minWarning() || value > info.maxWarning()) {
            // ... post warning
        }
    }

    // ================= Helpers ==============//

    private static boolean contains(int[] arr, int v) {
        for (int x : arr) if (x == v) return true;
        return false;
    }

    private static int ceilDiv(int a, int b) {
        if (b <= 0) return 0; 
        return (a + b - 1) / b;
    }

    void TelemetryUpdate(String msg, NotificationPanel.Status status) {
        SwingUtilities.invokeLater(() -> {
            notifications.post(status,
                NotificationPanel.Channel.TELEMETRY, msg);
        });
    }

    void VitalsUpdate(String msg, NotificationPanel.Status status) {
        SwingUtilities.invokeLater(() -> {
            notifications.post(status,
                NotificationPanel.Channel.VITALS, msg);
        });
    }

}
