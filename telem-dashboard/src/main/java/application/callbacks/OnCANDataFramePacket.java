/**
 * This is a user-editable file for handling CANDataFrame packets.
 * It is generated once and will not be overwritten.
 */
package application.callbacks;

import application.DataHandler;
import application.UI.NotificationPanel;
import lookup.TelemetryLookup;
import lookup.TelemetryRecords;
import presentation.TelemetryParserLUT;
import presentation.BitStream;
import presentation.TelemetryParser;

import java.io.EOFException;
import java.util.NoSuchElementException;

public class OnCANDataFramePacket {

    public void handle(TelemetryParserLUT.CANDataFramePacket p, BitStream stream, DataHandler dataHandler, NotificationPanel notifications, TelemetryLookup lookup) {
        int canNodeId = p.nodeID();
        int canFrameIndex = p.frameID();

        try {
            // 1. Look up the frame using the fixed CANDataFrame header fields
            TelemetryRecords.CANFrame frameInfo = lookup.getFrame(canNodeId, canFrameIndex);

            // 2. Use the generic parseFields helper to parse the data values for this CAN frame
            int[] parsedFrameValues = TelemetryParser.parseFields(stream, lookup, canNodeId, canFrameIndex, frameInfo.numData(), notifications);

            // 3. Process and plot the newly parsed CAN frame data
            System.out.println("[OnCANDataFramePacket] Parsed CANDataFrame: node=" + canNodeId
                    + " frame=" + canFrameIndex
                    + " values=" + java.util.Arrays.toString(parsedFrameValues));
            dataHandler.processAndPlotData(canNodeId, canFrameIndex, parsedFrameValues);
        } catch (NoSuchElementException | EOFException | IllegalArgumentException e) {
            System.out.println("[OnCANDataFramePacket] Failed to parse CANDataFrame for node "
                    + canNodeId + ": " + e.getMessage());
            notifications.post(NotificationPanel.Status.WARNING, NotificationPanel.Channel.TELEMETRY, "Failed to parse CANDataFrame content for node " + canNodeId + ": " + e.getMessage());
        }
    }
}
