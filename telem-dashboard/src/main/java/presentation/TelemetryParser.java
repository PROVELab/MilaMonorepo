package presentation;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.io.EOFException;
import java.util.NoSuchElementException;
import java.util.Optional;

import util.Constants;
import lookup.TelemetryRecords;
import application.UI.NotificationPanel;
import lookup.TelemetryLookup;
import presentation.TelemetryParserLUT.LutEntry;
import presentation.TelemetryParserLUT.PacketVisitor;
import presentation.TelemetryParserLUT.ParsedPacket;

/**
 * This is a static helper class for parsing the LoRa telemetry stream.
 * It uses the auto-generated TelemetryParserLUT.java for packet definitions and lookup tables.
 * This file is user-managed and is not overwritten by the code generator.
 */
public final class TelemetryParser {

    private TelemetryParser() {}

    public static Optional<TelemetryParserLUT.ParseResult> parseSinglePacket(byte[] loraPayload, TelemetryLookup lookup, NotificationPanel notifications, TelemetryParserLUT.PacketVisitor visitor) {
        if (loraPayload == null || loraPayload.length == 0) {
            return Optional.empty(); // No payload
        }
        BitStream stream = new BitStream(ByteBuffer.wrap(loraPayload).order(ByteOrder.LITTLE_ENDIAN));

        int startPosBits = stream.position();

        TelemetryParserLUT.ParsedPacket packet = parseNextFixed(stream, lookup, notifications); // Parse fixed part

        if (packet != null) {
            // The visitor is called to parse the custom payload. It modifies the stream directly.
            packet.accept(visitor, stream);

            // After the visitor is done, the stream is at the new position.
            // We report how many bytes were consumed from the original payload.
            int endPosBits = stream.position();
            int totalBitsConsumed = endPosBits - startPosBits;

            // The number of bytes consumed is the number of bits rounded up to the nearest byte.
            int bytesConsumed = (totalBitsConsumed + 7) / 8;
            return Optional.of(new TelemetryParserLUT.ParseResult(packet, bytesConsumed));
        }
        return Optional.empty(); // No packet parsed
    }

    private static TelemetryParserLUT.ParsedPacket parseNextFixed(BitStream stream, TelemetryLookup lookup, NotificationPanel notifications) {
        TelemetryParserLUT.LutEntry matchedEntry = null;
        try {
            for (TelemetryParserLUT.LutEntry entry : TelemetryParserLUT.LUT) {
                if (stream.peek(entry.bits) == entry.mask) {
                    matchedEntry = entry;
                    break;
                }
            }
        } catch (EOFException e) {
            // Not enough bits to peek for any packet, which is fine.
            // We'll fall through to the matchedEntry == null case.
        }

        if (matchedEntry == null) {
            String nextBits;
            try {
                nextBits = "0x" + Integer.toHexString(stream.peek(8));
            } catch (EOFException e) {
                nextBits = "?";
            }
            notifications.post(NotificationPanel.Status.WARNING, NotificationPanel.Channel.TELEMETRY, "TelemetryParser: No matching packet found for bits: " + nextBits + ". Remaining bytes: " + stream.remainingBytes());
            return null;
        }

        try {
            if (matchedEntry.bits > 0) {
                stream.read(matchedEntry.bits);
            }

            TelemetryParserLUT.ParsedPacket packet = matchedEntry.creator.create(); // Use the creator from LUT
            packet.packetIndex = matchedEntry.packetIndex;

            // Generic parsing for all fixed-field parts of packets.
            int[] values = packet.getValues();
            if (values.length > 0) {
                int[] parsedValues = parseFields(stream, lookup, util.Constants.specialIDs.vitalsID, packet.packetIndex, values.length, notifications);
                System.arraycopy(parsedValues, 0, values, 0, values.length);
            }
            return packet;
        } catch (EOFException | NoSuchElementException | IllegalArgumentException e) {
            notifications.post(NotificationPanel.Status.WARNING, NotificationPanel.Channel.TELEMETRY, "TelemetryParser: Error parsing packet '" + matchedEntry.name + "': " + e.getMessage());
            return null;
        }
    }

    /**
     * A generic function to parse a sequence of data fields from a bitstream based on lookup data.
     * This is used for parsing the fixed fields of telemetry packets and the data within a CAN frame.
     *
     * @param stream The bitstream to read from.
     * @param lookup The telemetry lookup service.
     * @param nodeId The ID of the node whose frame is being parsed.
     * @param frameIndex The index of the frame being parsed.
     * @param numFields The number of data fields to parse.
     * @param notifications A panel to post warnings to.
     * @return An array of parsed integer values.
     * @throws EOFException if the stream ends prematurely.
     * @throws NoSuchElementException if lookup fails for any field.
     * @throws IllegalArgumentException if data info is invalid (e.g., bit length).
     */
    public static int[] parseFields(BitStream stream, TelemetryLookup lookup, int nodeId, int frameIndex, int numFields, NotificationPanel notifications) throws EOFException, NoSuchElementException, IllegalArgumentException {
        if (numFields <= 0) {
            return new int[0];
        }
        int[] parsedValues = new int[numFields];
        for (int i = 0; i < numFields; i++) {
            TelemetryRecords.DataInfo dataInfo = lookup.getDataInfo(nodeId, frameIndex, i);
            if (dataInfo.bitLength() == 0) continue;

            int rawValue = stream.read(dataInfo.bitLength());
            // The value is sent using offset-binary encoding (original_value - min). To decode, we just add min back.
            // We must promote the rawValue to a long using a mask to correctly handle the full 32-bit unsigned range,
            // as Java's `int` is signed.
            parsedValues[i] = (int) ((rawValue & 0xFFFFFFFFL) + dataInfo.min());
        }
        return parsedValues;
    }
}