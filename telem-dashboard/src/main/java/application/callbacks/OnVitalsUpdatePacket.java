/**
 * This is a user-editable file for handling VitalsUpdate packets.
 * It is generated once and will not be overwritten.
 */
package application.callbacks;

import application.UI.NotificationPanel;
import application.DataHandler;
import lookup.TelemetryLookup;
import presentation.TelemetryParserLUT;
import util.Constants;
import util.IntConstUtils;

import javax.swing.SwingUtilities;

public class OnVitalsUpdatePacket {
    private static NotificationPanel.Entry busStatusEntry = null;

    public void handle(TelemetryParserLUT.VitalsUpdatePacket p, DataHandler dataHandler, NotificationPanel notifications, TelemetryLookup lookup) {
        System.out.println("[OnVitalsUpdatePacket] Parsed VitalsUpdate: TWAI_STATE=" + p.TWAI_STATE()
                + " HBMask=" + p.HBMask()
                + " slow1=(" + p.slowestNode1_ID() + "," + p.slowestNode1_time() + ")"
                + " slow2=(" + p.slowestNode2_ID() + "," + p.slowestNode2_time() + ")"
                + " slow3=(" + p.slowestNode3_ID() + "," + p.slowestNode3_time() + ")");
        // HB parsing
        // HBTiming and HBStatus did not have callback-specific behavior before being merged.

        // BUS Status parsing
        NotificationPanel.Status status;
        if(p.TWAI_STATE() == Constants.TWAI_STATE.TWAI_PECAN_RUNNING){
            status = NotificationPanel.Status.OK;
        }else{
            status = NotificationPanel.Status.WARNING;
        }

        String msg = "bus: state=" + IntConstUtils.nameFromInt(Constants.TWAI_STATE.class, p.TWAI_STATE())
                                    .orElse("Unknown state value (very bad)");

        SwingUtilities.invokeLater(() -> {
            if (busStatusEntry == null){
                busStatusEntry = notifications.post(status, NotificationPanel.Channel.VITALS, msg);
            } else {
                busStatusEntry.updateText(notifications, msg);
                busStatusEntry.updateStatus(status);
            }
        });
    }

}
