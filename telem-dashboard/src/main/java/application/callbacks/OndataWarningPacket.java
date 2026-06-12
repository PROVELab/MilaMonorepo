/**
 * This is a user-editable file for handling dataWarning packets.
 * It is generated once and will not be overwritten.
 */
package application.callbacks;

import application.DataHandler;
import application.UI.NotificationPanel;
import lookup.TelemetryLookup;
import presentation.TelemetryParserLUT;
import util.Constants;
import util.IntConstUtils;
public class OndataWarningPacket {

    public void handle(TelemetryParserLUT.dataWarningPacket p, DataHandler dataHandler, NotificationPanel notifications, TelemetryLookup lookup) {
        NotificationPanel.Status status;
        // Check if the trigger is for entering a warning range, which is less severe than a critical range.
        if (p.dataErrorTrigger() == Constants.dataErrorTrigger.enteredWarningRange) {
            status = NotificationPanel.Status.WARNING;
        } else {
            status = NotificationPanel.Status.CRITICAL;
        }

        StringBuilder msg = new StringBuilder();
        msg.append("Data Warning on Node '").append(lookup.getNodeName(p.nodeID())).append("'")
           .append(", Frame ").append(p.frameID())
           .append(", Data ").append(p.dataID())
           .append(": ");
        if (p.dataTooHigh() != 0) {
            msg.append("Data is too high. ");
        } else {
            msg.append("Data is too low. ");
        }

        String triggerName = IntConstUtils.nameFromInt(Constants.dataErrorTrigger.class, p.dataErrorTrigger())
                .orElse("Unknown trigger (" + p.dataErrorTrigger() + ")");
        msg.append("Trigger: ").append(triggerName).append(".");

        notifications.post(status, NotificationPanel.Channel.VITALS, msg.toString());
    }

}
