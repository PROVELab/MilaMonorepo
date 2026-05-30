/**
 * This is a user-editable file for handling frameWarning packets.
 * It is generated once and will not be overwritten.
 */
package application.callbacks;

import javax.swing.SwingUtilities;

import application.DataHandler;
import application.UI.NotificationPanel;
import lookup.TelemetryLookup;
import presentation.TelemetryParserLUT;
import util.Constants;
import util.IntConstUtils;

public class OnframeWarningPacket {

    public void handle(TelemetryParserLUT.frameWarningPacket p, DataHandler dataHandler, NotificationPanel notifications, TelemetryLookup lookup) {
        // All frame warnings are considered critical.
        NotificationPanel.Status status = NotificationPanel.Status.CRITICAL;

        StringBuilder msg = new StringBuilder();
        msg.append("Frame Warning on Node ").append(p.nodeID())
           .append(", Frame ").append(p.frameID())
           .append(": ");

        String triggerName = IntConstUtils.nameFromInt(Constants.frameErrorTrigger.class, p.frameErrorTrigger())
                .orElse("Unknown trigger (" + p.frameErrorTrigger() + ")");
        msg.append("Trigger: ").append(triggerName).append(".");

        SwingUtilities.invokeLater(() -> {
            notifications.post(status, NotificationPanel.Channel.VITALS, msg.toString());
        });
    }

}
