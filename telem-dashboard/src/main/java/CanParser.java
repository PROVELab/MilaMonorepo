import java.util.Map;
import java.io.EOFException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.io.IOException;
import java.util.List;
import java.util.NoSuchElementException;
import java.util.concurrent.ConcurrentHashMap;

import javax.swing.Timer;

import application.UI.MainFrame;
import application.UI.MainPanel;
import application.UI.NotificationPanel;
import application.UI.SensorSelectionPanel;
import lookup.TelemetryLookup;
import presentation.BitStream;
import presentation.TelemetryParser;

import javax.swing.SwingUtilities;

//Parses Can messages and updates display. Also formats user messages to Can before sending to telem
public class CanParser {

    private final TelemetryLookup lookup;
    private final NotificationPanel notifications;
    private final MainPanel mainPanel;
    private final MainFrame mainFrame;
    private SerialBridge sb;
    private final GeneratedPacketVisitor packetCallbacks;
    private final CommandParser commandParser;
    private final GeneratedCANFrameVisitor canFrameCallbacks;
    private final Map<TelemetryLookup.FrameKey, Timer> timeoutTimers = new ConcurrentHashMap<>();

    public CanParser(TelemetryLookup lookup, NotificationPanel notifications, MainPanel mainPanel, MainFrame mainFrame) {
        this.lookup = lookup;
        this.notifications = notifications;
        this.mainPanel=mainPanel;
        this.mainFrame = mainFrame;
        this.packetCallbacks = new GeneratedPacketVisitor(lookup, notifications, mainPanel);
        this.canFrameCallbacks = new GeneratedCANFrameVisitor(lookup, notifications, mainPanel);

        // Create the command parser and install it into the notification panel
        this.commandParser = new CommandParser(this.notifications, this::sendCommand);
        this.notifications.setCommandPanel(this.commandParser);

        System.out.println("Can init");
        final String portName = "/dev/ttyUSB0"; final int baud = 115200;

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

    }

    private void sendCommand(byte[] payload) {
        try {
            if (sb.isConnected()) {
                sb.sendMessage(payload);
            } else {
                TelemetryUpdate("Cannot send command: Serial port not connected.", NotificationPanel.Status.CRITICAL);
            }
        } catch (IOException e) {
            TelemetryUpdate("Failed to send command: " + e.getMessage(), NotificationPanel.Status.CRITICAL);
        }
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

    private int onMessageRecv(byte[] loraPayload) {
        if (loraPayload == null || loraPayload.length == 0) {
            TelemetryUpdate("onMessageRecv got empty payload!", NotificationPanel.Status.WARNING);
            return 0;
        }

        // This method is now called in a loop by SerialBridge.
        // It should parse ONE packet from the start of the payload and return its length in bytes.
        // The visitor is passed in to handle custom payload parsing.
        TelemetryParserLUT.ParseResult result = TelemetryParser.parseSinglePacket(loraPayload, this.lookup, this.notifications, this.packetCallbacks); // Use static TelemetryParser

        if (result != null && result.packet != null) {
            // The packet's fixed fields are parsed, and its custom payload (if any)
            // has been parsed by the visitor during the call above.
            // Now, we can process the fully-formed packet.
            TelemetryParserLUT.ParsedPacket packet = result.packet;
            if (packet instanceof TelemetryParserLUT.CANDataFramePacket p) {
                processCANDataFrame(p);
            } else {
                // This branch handles special, non-CAN packets (like HBTimingPacket, vitalsErr, etc.)
                // that are still defined in TelemetryParser and have plottable data associated with the 'vitals' node.
                processPlottablePacket(packet);
            }
            return result.bytesConsumed;
        }
        return 0;
    }

    private void processCANDataFrame(TelemetryParserLUT.CANDataFramePacket p) {
        BitStream payloadStream = new BitStream(ByteBuffer.wrap(p.payload).order(ByteOrder.LITTLE_ENDIAN));
        int nodeId = p.nodeID();

        try {
            TelemetryRecords.Node nodeInfo = lookup.getNodeById(nodeId);
            TelemetryRecords.CANFrame frame = lookup.lookupFrameFromStream(payloadStream, nodeInfo);
            int frameIndex = frame.frameIndex();

           
            int[] parsedValues = new int[frame.numData()];

            for (int i = 0; i < frame.numData(); i++) {
                TelemetryRecords.DataInfo dataInfo = lookup.getDataInfo(nodeId, frameIndex, i);

                if (dataInfo.bitLength() < 0 || dataInfo.bitLength() > 32) {
                    TelemetryUpdate("Invalid bitLength for CAN data. Node: " + nodeId + " Frame: " + frameIndex + " DataIndex: " + i, NotificationPanel.Status.WARNING);
                    return;
                }

                int rawValue;
                if (dataInfo.bitLength() > 0) {
                    rawValue = payloadStream.read(dataInfo.bitLength());
                } else {
                    TelemetryUpdate("DataInfo bitLength is 0, (something is probably cooked). node: " + nodeId + " Frame: " + frameIndex + " DataIndex: " + i, NotificationPanel.Status.WARNING);
                    rawValue = 0;
                }
                // The value is sent using offset-binary encoding (original_value - min). To decode, we just add min back.
                // We must promote the rawValue to a long using a mask to correctly handle the full 32-bit unsigned range,
                // as Java's `int` is signed. This ensures the addition is correct even when the unsigned value from C
                // would be interpreted as a negative number in Java. The final result is cast back to int, which is safe
                // because the original value was a signed 32-bit int.
                parsedValues[i] = (int) ((rawValue & 0xFFFFFFFFL) + dataInfo.min());
            }
            processAndPlotData(nodeId, frameIndex, parsedValues);
        } catch (NoSuchElementException e) {
            TelemetryUpdate("CAN data lookup failed for node " + nodeId + ". Frame may be unknown or malformed. error msg: " + e.getMessage(), NotificationPanel.Status.WARNING);
        } catch (EOFException e) {
            TelemetryUpdate("CAN data stream ended prematurely for node " + nodeId + ".", NotificationPanel.Status.WARNING);
        } catch (IllegalArgumentException e) {
            TelemetryUpdate("Invalid argument while parsing CAN data for node " + nodeId + ": " + e.getMessage(), NotificationPanel.Status.WARNING);
        }
    }

    private void processPlottablePacket(TelemetryParserLUT.ParsedPacket packet) {
        int nodeId = Constants.specialIDs.vitalsID;
        int frameIndex = packet.packetIndex;
        int[] values = packet.getValues();
        processAndPlotData(nodeId, frameIndex, values);
    }


    /**
     * The shared logic for processing a fully parsed frame of data values.
     * This method handles timer resets, data validation, plotting, and invoking CAN frame callbacks.
     * @param nodeId The node ID for the frame.
     * @param frameIndex The frame index for the frame.
     * @param values The array of parsed integer data values for the frame.
     */
    private void processAndPlotData(int nodeId, int frameIndex, int[] values) {
        try {
            TelemetryRecords.CANFrame frame = lookup.getFrame(nodeId, frameIndex);

            if (frame.dataTimeout() > 0) { onFrameReceivedResetTimer(nodeId, frameIndex, frame.dataTimeout()); }

            if (values.length != frame.numData()) {
                TelemetryUpdate("Mismatched data count for frame. Node: " + nodeId + " Frame: " + frameIndex
                        + ". Expected " + frame.numData() + ", got " + values.length, NotificationPanel.Status.CRITICAL);
                return;
            }

            for (int i = 0; i < frame.numData(); i++) {
                TelemetryLookup.DataKey dataKey = new TelemetryLookup.DataKey(nodeId, frameIndex, i);
                TelemetryRecords.DataInfo dataInfo = lookup.getDataInfo(dataKey);

                int dataValue = values[i];

                checkDataValue(dataKey, dataInfo, dataValue);
                if (!mainPanel.addDataPoint(nodeId, frameIndex, i, dataValue)) {
                    TelemetryUpdate("Failed to add data point to main panel. Node: " + nodeId + " Frame: " + frameIndex
                            + " DataIndex: " + i + " Value: " + dataValue, NotificationPanel.Status.WARNING);
                }
            }

            if (frame.enableTelemCallback()) {
                CANFrameParser.ParsedCANFrame canPacket = CANFrameParser.createPacket(nodeId, frameIndex, values);
                if (canPacket != null) {
                    canPacket.accept(this.canFrameCallbacks);
                }
            }
        } catch (NoSuchElementException e) {
            String nodeName = lookup.getNodeNameOpt(nodeId).orElse("ID " + nodeId);
            String msg = String.format("Failed to process data for %s (frameIndex=%d): Lookup failed. %s",
                                       nodeName, frameIndex, e.getMessage());
            TelemetryUpdate(msg, NotificationPanel.Status.WARNING);
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
        try {
            int expectedTimeout = lookup.getFrame(key.nodeId(), key.frameIndex()).dataTimeout();
            String nodeName = lookup.getNodeName(key.nodeId());
            String msg = String.format("Missing frame: %s (frameIndex=%d). Expected every ~%dms.",
                            nodeName,
                            key.frameIndex(),
                            expectedTimeout);

            TelemetryUpdate(msg, NotificationPanel.Status.WARNING);
        } catch (NoSuchElementException e) {
            TelemetryUpdate("Frame timeout for unknown frame. exception: " + e.getMessage(), NotificationPanel.Status.WARNING);            
        }
    }
    
    private void checkDataValue(TelemetryLookup.DataKey key, TelemetryRecords.DataInfo info, int value) {
        String title = lookup.titleFor(key);
        if (value < info.minCritical() || value > info.maxCritical()) {
            String msg = String.format("CRITICAL: %s is %d (out of range [%d, %d])",
                                       title, value, info.minCritical(), info.maxCritical());
            TelemetryUpdate(msg, NotificationPanel.Status.CRITICAL);
        } else if (value < info.minWarning() || value > info.maxWarning()) {
            String msg = String.format("WARNING: %s is %d (out of range [%d, %d])",
                                       title, value, info.minWarning(), info.maxWarning());
            TelemetryUpdate(msg, NotificationPanel.Status.WARNING);
        }
        // Also update the sensor status indicator in the left panel
        SensorSelectionPanel.setStatusIndicator(lookup, key, info, value);
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
