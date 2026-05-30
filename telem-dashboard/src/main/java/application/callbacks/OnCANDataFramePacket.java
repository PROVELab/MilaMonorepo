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

        try {
            // 1. Look up the node to determine how to parse the frame index
            TelemetryRecords.Node nodeInfo = lookup.getNodeById(canNodeId);

            // 2. Read the CAN frame index from the stream
            TelemetryRecords.CANFrame frameInfo = lookup.lookupFrameFromStream(stream, nodeInfo);
            int canFrameIndex = frameInfo.frameIndex();

            // 3. Use the generic parseFields helper to parse the data values for this CAN frame
            int[] parsedFrameValues = TelemetryParser.parseFields(stream, lookup, canNodeId, canFrameIndex, frameInfo.numData(), notifications);

            // 4. Process and plot the newly parsed CAN frame data
            dataHandler.processAndPlotData(canNodeId, canFrameIndex, parsedFrameValues);
        } catch (NoSuchElementException | EOFException | IllegalArgumentException e) {
            notifications.post(NotificationPanel.Status.WARNING, NotificationPanel.Channel.TELEMETRY, "Failed to parse CANDataFrame content for node " + canNodeId + ": " + e.getMessage());
        }
    }
}
