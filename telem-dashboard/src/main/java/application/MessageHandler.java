package application;
import java.util.Map;
import java.io.EOFException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.io.IOException;
import java.util.List;
import java.util.NoSuchElementException;
import java.util.concurrent.ConcurrentHashMap;

import javax.swing.Timer;

import util.Constants;
import util.SerialBridge;
import lookup.TelemetryRecords;
import application.UI.CommandPanel;
import application.UI.MainFrame;
import application.UI.MainPanel;
import application.UI.NotificationPanel;
import application.UI.SensorSelectionPanel;
import lookup.TelemetryLookup;
import presentation.BitStream;
import presentation.CANFrameParser;
import presentation.GeneratedCANFrameVisitor;
import presentation.GeneratedPacketVisitor;
import presentation.TelemetryParser;
import presentation.TelemetryParserLUT.CANDataFramePacket;
import presentation.TelemetryParserLUT.ParseResult;
import presentation.TelemetryParserLUT.ParsedPacket;

import javax.swing.SwingUtilities;

//Parses Can messages and updates display. Also formats user messages to Can before sending to telem
public class MessageHandler {

    private final TelemetryLookup lookup;
    private final NotificationPanel notifications;
    private final MainPanel mainPanel;
    private final MainFrame mainFrame;
    private SerialBridge sb;
    private final GeneratedPacketVisitor packetCallbacks;
    private final CommandPanel commandParser;
    private final GeneratedCANFrameVisitor canFrameCallbacks;
    private final Map<TelemetryLookup.FrameKey, Timer> timeoutTimers = new ConcurrentHashMap<>();
    private final DataHandler dataHandler;

    public MessageHandler(TelemetryLookup lookup, NotificationPanel notifications, MainPanel mainPanel, MainFrame mainFrame, String logFileName) {
        this.lookup = lookup;
        this.notifications = notifications;
        this.mainPanel=mainPanel;
        this.mainFrame = mainFrame;
        this.canFrameCallbacks = new GeneratedCANFrameVisitor(lookup, notifications, mainPanel);
        this.dataHandler = new DataHandler(lookup, notifications, mainPanel, this.canFrameCallbacks, this.timeoutTimers);
        this.packetCallbacks = new GeneratedPacketVisitor(lookup, notifications, this.dataHandler);

        // Create the command parser and install it into the notification panel
        this.commandParser = new CommandPanel(this.notifications, this::sendCommand);
        this.notifications.setCommandPanel(this.commandParser);

        System.out.println("Can init");
        final String portName = "/dev/ttyUSB0"; final int baud = 115200;

        // Pass the message, status, and log file callbacks to the SerialBridge
        this.sb = new SerialBridge(portName, baud, this::onMessageRecv, this::updateSerialStatus, logFileName);

        // Initial connection attempt
        // Run in a new thread to avoid blocking the UI during initial sleep in SerialBridge
        new Thread(() -> {
            if (!this.sb.connect()) {
                notifications.TelemetryUpdate("Initial SerialBridge connection failed.", NotificationPanel.Status.CRITICAL);
            }
        }).start();

        mainFrame.setCanParser(this);

    }

    private void sendCommand(byte[] payload) {
        try {
            if (sb.isConnected()) {
                sb.sendMessage(payload);
            } else {
                notifications.TelemetryUpdate("Cannot send command: Serial port not connected.", NotificationPanel.Status.CRITICAL);
            }
        } catch (IOException e) {
            notifications.TelemetryUpdate("Failed to send command: " + e.getMessage(), NotificationPanel.Status.CRITICAL);
        }
    }

    // New method to handle status updates from SerialBridge
    private void updateSerialStatus(boolean connected) {
        SwingUtilities.invokeLater(() -> mainFrame.setSerialStatus(connected));
        if (connected) {
            notifications.TelemetryUpdate("Serial port connected.", NotificationPanel.Status.OK);
        } else {
            // This message can be noisy if it keeps trying and failing, but it's useful.
            notifications.TelemetryUpdate("Serial port disconnected.", NotificationPanel.Status.CRITICAL);
        }
    }

    // New public method for the UI to call
    public void restartSerialConnection() {
        notifications.TelemetryUpdate("Attempting to restart serial connection...", NotificationPanel.Status.OK);
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
                notifications.TelemetryUpdate("SerialBridge restart failed.", NotificationPanel.Status.CRITICAL);
            }
        }).start();
    }

    private void onMessageRecv(byte[] uartPayload) {
        // New payload format from ESP32: [RSSI(4)][SNR(4)][irqFlags(4)][dataSize(4)][data(dataSize)]
        // All values are little-endian.
        final int METADATA_SIZE = 16; // sizeof(float)*2 + sizeof(size_t)*2
        if (uartPayload.length < METADATA_SIZE) {
            notifications.TelemetryUpdate("UART payload too short for metadata: " + uartPayload.length, NotificationPanel.Status.CRITICAL);
            return;
        }

        ByteBuffer bb = ByteBuffer.wrap(uartPayload).order(ByteOrder.LITTLE_ENDIAN);

        byte[] telemetryPayload = parseAndLogHeaders(bb);

        if (telemetryPayload == null) {
            // An error occurred during header parsing, already notified.
            return;
        }

        // The data processing and UI updates must happen on the Event Dispatch Thread (EDT)
        // to avoid deadlocks and keep the UI responsive. The serial-reader thread, which
        // calls this method, is now free to immediately read the next message.
        SwingUtilities.invokeLater(() -> {
            // The telemetryPayload contains one or more telemetry packets.
            // The TelemetryParser expects a stream of telemetry packets.
            int totalBytesConsumedInPayload = 0;
            while (totalBytesConsumedInPayload < telemetryPayload.length) {
                byte[] remainingPayload = new byte[telemetryPayload.length - totalBytesConsumedInPayload];
                System.arraycopy(telemetryPayload, totalBytesConsumedInPayload, remainingPayload, 0, remainingPayload.length);

                System.out.println("[MessageHandler] Parsing next packet from payload...");
                ParseResult result = TelemetryParser.parseSinglePacket(remainingPayload, this.lookup, this.notifications, this.packetCallbacks);
                System.out.println("[MessageHandler] Parser returned. Consumed: " + result.bytesConsumed + " bytes. Packet: " + (result.packet != null ? result.packet.packetName : "null"));

                if (result.packet != null) {
                    // The visitor pattern has already dispatched to the correct handler.
                    // We only need to handle special, non-CAN packets here.
                    if (!(result.packet instanceof CANDataFramePacket)) {
                        System.out.println("[MessageHandler] Processing special packet: " + result.packet.packetName);
                        // For non-CAN packets, the visitor is a no-op. We process their fixed-field values directly.
                        dataHandler.processAndPlotData(Constants.specialIDs.telemetryID, result.packet.packetIndex, result.packet.getValues());
                        System.out.println("[MessageHandler] Done processing special packet: " + result.packet.packetName);
                    } else {
                        System.out.println("[MessageHandler] CANDataFrame packet handled by visitor.");
                    }
                }

                if (result.bytesConsumed > 0) {
                    totalBytesConsumedInPayload += result.bytesConsumed;
                } else {
                    if (remainingPayload.length > 0) {
                        notifications.TelemetryUpdate("Parser stalled in telemetry payload. Discarding 1 byte to prevent infinite loop.", NotificationPanel.Status.CRITICAL);
                        totalBytesConsumedInPayload += 1; // Advance to avoid infinite loop
                    } else {
                        break; // No more data
                    }
                }
            }
        });
    }

    /**
     * Parses and logs the metadata and LoRa protocol headers from the UART payload.
     * @param uartBuffer The ByteBuffer wrapping the UART payload.
     * @return The extracted telemetry data payload, or null if an error occurred.
     */
    private byte[] parseAndLogHeaders(ByteBuffer uartBuffer) {
        // 1. Parse and log metadata
        float rssi = uartBuffer.getFloat();
        float snr = uartBuffer.getFloat();
        int irqFlags = uartBuffer.getInt();
        int loraDataSize = uartBuffer.getInt();

        System.out.println(String.format("[MessageHandler] Recv metadata: RSSI=%.2f, SNR=%.2f, IRQ=0x%X, LoRaSize=%d",
                                         rssi, snr, irqFlags, loraDataSize));

        if (loraDataSize < 0 || loraDataSize > uartBuffer.remaining()) {
            notifications.TelemetryUpdate("Invalid LoRa data size in UART payload: " + loraDataSize + ", remaining: " + uartBuffer.remaining(), NotificationPanel.Status.CRITICAL);
            return null; // Indicate error
        }

        // 2. Parse and log LoRa protocol header from within the LoRa data
        // blastProtocolConfig.hpp: TXProtocolPacket
        //   protocolID_t protocolID;    // uint16_t
        //   tx_flags_t flags;           // uint8_t
        //   tx_frameTrack_t frameNum;   // uint8_t
        final int TX_HEADER_SIZE = 2 + 1 + 1;
        if (loraDataSize < TX_HEADER_SIZE) {
            notifications.TelemetryUpdate("LoRa payload too short for TX header: " + loraDataSize, NotificationPanel.Status.CRITICAL);
            return null; // Indicate error
        }

        int protocolID = uartBuffer.getShort() & 0xFFFF; // Read 2 bytes as short, convert to unsigned int
        byte flags = uartBuffer.get();
        byte frameTrack = uartBuffer.get();

        // From blastProtocolConfig.hpp FrameTrack namespace
        int frameNum = frameTrack & 0x0F;
        int burstSize = (frameTrack & 0xF0) >> 4;

        System.out.println(String.format("[MessageHandler] Recv LoRa Header: ProtoID=0x%X, Flags=0x%X, FrameNum=%d, BurstSize=%d",
                                         protocolID, flags, frameNum, burstSize));
        
        // Check for unique protocol ID
        final int PROTOCOL_UNIQUE_ID = 0x9354;
        if (protocolID != PROTOCOL_UNIQUE_ID) {
            System.err.println(String.format("[MessageHandler] WARNING: Mismatched protocol ID! Expected 0x%X, got 0x%X",
                                             PROTOCOL_UNIQUE_ID, protocolID));
        }

        // 3. Extract and return the actual telemetry payload
        int telemetryPayloadSize = loraDataSize - TX_HEADER_SIZE;
        byte[] telemetryPayload = new byte[telemetryPayloadSize];
        uartBuffer.get(telemetryPayload);

        return telemetryPayload;
    }
}
