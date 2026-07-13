/**
 * This is a fully auto-generated file. DO NOT EDIT.
 * It implements the PacketVisitor and delegates each packet to a handler
 * in the 'callbacks' package. Skeletons for those handlers are generated
 * once and are safe for user modification.
 */
package presentation;
import presentation.BitStream;

import application.UI.NotificationPanel;
import application.DataHandler;
import application.callbacks.*;
import lookup.TelemetryLookup;
public class GeneratedPacketVisitor implements TelemetryParserLUT.PacketVisitor {

    private final TelemetryLookup lookup;
    private final NotificationPanel notifications;
    private final DataHandler dataHandler;

    public GeneratedPacketVisitor(TelemetryLookup lookup, NotificationPanel notifications, DataHandler dataHandler) {
        this.lookup = lookup;
        this.notifications = notifications;
        this.dataHandler = dataHandler;
    }

    @Override
    public void visit(TelemetryParserLUT.VitalsUpdatePacket p) {
        new OnVitalsUpdatePacket().handle(p, dataHandler, notifications, lookup);
        return;
    }

    @Override
    public void visit(TelemetryParserLUT.vitalsErrPacket p, BitStream stream) {
        new OnvitalsErrPacket().handle(p, stream, dataHandler, notifications, lookup);
    }

    @Override
    public void visit(TelemetryParserLUT.dataWarningPacket p) {
        new OndataWarningPacket().handle(p, dataHandler, notifications, lookup);
        return;
    }

    @Override
    public void visit(TelemetryParserLUT.frameWarningPacket p) {
        new OnframeWarningPacket().handle(p, dataHandler, notifications, lookup);
        return;
    }

    @Override
    public void visit(TelemetryParserLUT.nodeStatusPacket p) {
        new OnnodeStatusPacket().handle(p, dataHandler, notifications, lookup);
        return;
    }

    @Override
    public void visit(TelemetryParserLUT.unknownCanPacketPacket p, BitStream stream) {
        new OnunknownCanPacketPacket().handle(p, stream, dataHandler, notifications, lookup);
    }

    @Override
    public void visit(TelemetryParserLUT.CANDataFramePacket p, BitStream stream) {
        new OnCANDataFramePacket().handle(p, stream, dataHandler, notifications, lookup);
    }

    @Override
    public void visit(TelemetryParserLUT.ParsedPacket p) {
        // Fallback for unhandled packets
    }
}
