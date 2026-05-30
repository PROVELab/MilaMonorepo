/**
 * This is a user-editable file for handling BusStatus packets.
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
public class OnBusStatusPacket {
    private NotificationPanel.Entry BusStatusEntry = null;
    public void handle(TelemetryParserLUT.BusStatusPacket p, DataHandler dataHandler, NotificationPanel notifications, TelemetryLookup lookup) {
        //maintain a notification for vitals TWAI bus state. Give warning if bus is not running.
        NotificationPanel.Status status;
        if(p.TWAI_STATE() == Constants.TWAI_STATE.TWAI_PECAN_RUNNING){
            status = NotificationPanel.Status.OK;
        }else{
            status = NotificationPanel.Status.WARNING;
        }

        String msg = "bus: state=" + IntConstUtils.nameFromInt(Constants.TWAI_STATE.class, p.TWAI_STATE())
                                    .orElse("Unknown state value (very bad)");

        SwingUtilities.invokeLater(() -> {
            if (BusStatusEntry == null){
                BusStatusEntry = notifications.post(status, NotificationPanel.Channel.VITALS, msg);
            } else {
                BusStatusEntry.updateText(notifications, msg);
                BusStatusEntry.updateStatus(status);
            }
        });
    }
}
