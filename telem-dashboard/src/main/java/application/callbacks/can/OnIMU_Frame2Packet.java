/**
 * This is a user-editable file for handling CAN frames.
 * It is generated once and will not be overwritten.
 */
package application.callbacks.can;

import application.UI.MainPanel;
import application.UI.NotificationPanel;
import lookup.TelemetryLookup;
import presentation.CANFrameParser;

public class OnIMU_Frame2Packet {

    public void handle(CANFrameParser.IMU_Frame2Packet p, MainPanel mainPanel, NotificationPanel notifications, TelemetryLookup lookup) {
        // This callback is fired when a CAN frame with 'enableTelemCallback=true' is received.
        // The data has already been parsed, plotted, and checked for timeouts.
        // You can access the data via p.dataName() methods, e.g., p.accelX_miliGs()
        
    }

}
