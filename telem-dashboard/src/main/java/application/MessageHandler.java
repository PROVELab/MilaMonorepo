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

    public MessageHandler(TelemetryLookup lookup, NotificationPanel notifications, MainPanel mainPanel, MainFrame mainFrame) {
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

        // Pass the status update callback to the SerialBridge
        this.sb = new SerialBridge(portName, baud, this::onMessageRecv, this::updateSerialStatus);

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

    //returns number of bytes parsed from the payload
    private int onMessageRecv(byte[] loraPayload) {
        java.util.Optional<ParseResult> resultOpt = TelemetryParser.parseSinglePacket(loraPayload, this.lookup, this.notifications, this.packetCallbacks);

        if (resultOpt.isEmpty()) {
            if (loraPayload == null || loraPayload.length == 0) {
                notifications.TelemetryUpdate("onMessageRecv got empty payload!", NotificationPanel.Status.WARNING);
            }
            return 0; // No packet parsed, or an error occurred during parsing.
        }

        ParseResult result = resultOpt.get();

        // The visitor pattern has already dispatched to the correct handler.
        // OnCANDataFramePacket now calls processAndPlotData itself.
        // We only need to handle the other packets here.
        if (!(result.packet instanceof CANDataFramePacket)) {
                // This branch handles special, non-CAN packets (like HBTimingPacket, vitalsErr, etc.)
                // that are still defined in TelemetryParser and have plottable data associated with the 'vitals' node.
                dataHandler.processAndPlotData(Constants.specialIDs.vitalsID, result.packet.packetIndex, result.packet.getValues());
        }

        return result.bytesConsumed;
    }
}
