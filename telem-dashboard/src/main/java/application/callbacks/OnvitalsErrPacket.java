/**
 * This is a user-editable file for handling vitalsErr packets.
 * It is generated once and will not be overwritten.
 */
package application.callbacks;

import application.DataHandler;
import application.UI.NotificationPanel;
import lookup.TelemetryLookup;
import presentation.TelemetryParserLUT;
import presentation.BitStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.Optional;

public class OnvitalsErrPacket {

    public void handle(TelemetryParserLUT.vitalsErrPacket p, BitStream stream, DataHandler dataHandler, NotificationPanel notifications, TelemetryLookup lookup) {
        // TODO: Implement any additional logic for vitalsErr
        for (int i = 0; i < p.numErrors(); i++) {
            Optional<Short> errorCode = stream.readShort("error_code_");
            errorCode.ifPresentOrElse(
                code -> notifications.post(NotificationPanel.Status.WARNING, NotificationPanel.Channel.VITALS, "Vitals Error Code: " + code),
                () -> notifications.post(NotificationPanel.Status.WARNING, NotificationPanel.Channel.VITALS, "Failed to read error code")
            );
        }
    }

}
