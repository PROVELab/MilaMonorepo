/** Auto-generated file. Do not edit. */

package presentation;

import application.UI.MainPanel;
import application.UI.NotificationPanel;
import application.callbacks.can.*;
import lookup.TelemetryLookup;

public class GeneratedCANFrameVisitor implements CANFrameParser.CANFrameVisitor {

    private final TelemetryLookup lookup;
    private final NotificationPanel notifications;
    private final MainPanel mainPanel;

    public GeneratedCANFrameVisitor(TelemetryLookup lookup, NotificationPanel notifications, MainPanel mainPanel) {
        this.lookup = lookup;
        this.notifications = notifications;
        this.mainPanel = mainPanel;
    }

    @Override
    public void visit(CANFrameParser.IMU_Frame1Packet p) {
        new OnIMU_Frame1Packet().handle(p, mainPanel, notifications, lookup);
    }

    @Override
    public void visit(CANFrameParser.IMU_Frame2Packet p) {
        new OnIMU_Frame2Packet().handle(p, mainPanel, notifications, lookup);
    }

    @Override
    public void visit(CANFrameParser.IMU_Frame3Packet p) {
        new OnIMU_Frame3Packet().handle(p, mainPanel, notifications, lookup);
    }

}
