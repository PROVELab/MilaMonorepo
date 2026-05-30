/**
 * This is a user-editable file for handling unknownCanPacket packets.
 * It is generated once and will not be overwritten.
 */
package application.callbacks;

import application.DataHandler;
import application.UI.NotificationPanel;
import lookup.TelemetryLookup;
import presentation.TelemetryParserLUT;
import presentation.BitStream;
import java.io.EOFException;
import java.util.Optional;

public class OnunknownCanPacketPacket {

    public void handle(TelemetryParserLUT.unknownCanPacketPacket p, BitStream stream, DataHandler dataHandler, NotificationPanel notifications, TelemetryLookup lookup) {
        StringBuilder msg = new StringBuilder();
        msg.append("Vitals reported an unknown CAN packet. Details: ");

        long canId = p.nodeID();

        if (p.extendedIDPresent() != 0) {
            try {
                int extIdPart = stream.read(16);
                long extension = (long)(p.ext_id_start() << 16) | extIdPart;
                canId = (extension << 11) | p.nodeID();
                msg.append(String.format("Extended ID: 0x%X, ", canId));
            } catch (EOFException e) {
                notifications.post(NotificationPanel.Status.WARNING, NotificationPanel.Channel.TELEMETRY, "Failed to read extended ID for unknown CAN packet.");
                return;
            }
        } else {
            msg.append(String.format("Standard ID: 0x%X, ", canId));
        }

        msg.append(String.format("DLC: %d, ", p.DLC()));
        msg.append(p.RTR() != 0 ? "RTR, " : "Data, ");

        if (p.RTR() == 0 && p.DLC() > 0) {
            Optional<byte[]> data = stream.readBytes(p.DLC(), "can_data");
            data.ifPresentOrElse(
                bytes -> {
                    msg.append("Payload: ");
                    for (byte b : bytes) {
                        msg.append(String.format("%02X ", b));
                    }
                },
                () -> msg.append("Failed to read payload. ")
            );
        }

        notifications.post(NotificationPanel.Status.WARNING, NotificationPanel.Channel.TELEMETRY, msg.toString().trim());
    }

}
