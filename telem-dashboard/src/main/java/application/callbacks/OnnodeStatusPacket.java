/**
 * This is a user-editable file for handling nodeStatus packets.
 * It is generated once and will not be overwritten.
 */
package application.callbacks;

import application.DataHandler;
import application.UI.NotificationPanel;
import lookup.TelemetryLookup;
import util.Constants;
import util.IntConstUtils;
import presentation.TelemetryParserLUT;
public class OnnodeStatusPacket {

    public void handle(TelemetryParserLUT.nodeStatusPacket p, DataHandler dataHandler, NotificationPanel notifications, TelemetryLookup lookup) {
        String nodeName = lookup.getNodeNameOpt(p.nodeID()).orElse("Unknown Node " + p.nodeID());
        String statusName = IntConstUtils.nameFromInt(Constants.statusUpdates.class, p.statusUpdates())
                .orElse("Unknown status (" + p.statusUpdates() + ")");

        String msg = String.format("Node '%s' reported status: %s", nodeName, statusName);
        notifications.post(NotificationPanel.Status.OK, NotificationPanel.Channel.VITALS, msg);
    }

}
