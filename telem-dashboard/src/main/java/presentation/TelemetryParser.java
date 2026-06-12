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

    public static TelemetryParserLUT.ParseResult parseSinglePacket(byte[] loraPayload, TelemetryLookup lookup, NotificationPanel notifications, TelemetryParserLUT.PacketVisitor visitor) {
        if (loraPayload == null || loraPayload.length == 0) {
            return new TelemetryParserLUT.ParseResult(null, 0); // No payload, consume 0 bytes
        }
        BitStream stream = new BitStream(ByteBuffer.wrap(loraPayload).order(ByteOrder.LITTLE_ENDIAN));
        TelemetryParserLUT.LutEntry matchedEntry = null;
        try {
            for (TelemetryParserLUT.LutEntry entry : TelemetryParserLUT.LUT) {
                if (stream.peek(entry.bits) == entry.mask) {
                    matchedEntry = entry;
                    break;
                }
            }
        } catch (EOFException e) {
            // Not enough data to even peek for a header. Consume what's left.
            return new TelemetryParserLUT.ParseResult(null, loraPayload.length);
        }

        if (matchedEntry == null) {
            String nextBits;
            try {
                nextBits = "0x" + Integer.toHexString(stream.peek(8));
            } catch (EOFException e) {
                nextBits = "?";
            }
            notifications.post(NotificationPanel.Status.WARNING, NotificationPanel.Channel.TELEMETRY, "TelemetryParser: No matching packet found for bits: " + nextBits + ". Remaining bytes: " + stream.remainingBytes());
            // Consume 1 byte to prevent infinite loop
            return new TelemetryParserLUT.ParseResult(null, 1);
        }

        System.out.println("[Telemetry Parser] Matched packet: " + matchedEntry.name);
        int startPosBits = stream.position();
        try {
            if (matchedEntry.bits > 0) {
                stream.read(matchedEntry.bits);
            }

            ParsedPacket packet = matchedEntry.creator.create(); // Use the creator from LUT
            packet.packetIndex = matchedEntry.packetIndex;

            // Generic parsing for all fixed-field parts of packets.
            int[] values = packet.getValues();
            if (values.length > 0) {
                int[] parsedValues = parseFields(stream, lookup, util.Constants.specialIDs.telemetryID, packet.packetIndex, values.length, notifications);
                System.arraycopy(parsedValues, 0, values, 0, values.length);
            }

            // The visitor is called to parse the custom payload. It modifies the stream directly.
            packet.accept(visitor, stream);

            // To correctly account for packets that are not a multiple of 8 bits long,
            // we calculate consumed bytes by rounding up the number of bits consumed.
            int bitsConsumed = stream.position() - startPosBits;
            int bytesConsumed = (bitsConsumed + 7) / 8;

            return new TelemetryParserLUT.ParseResult(packet, bytesConsumed);

        } catch (EOFException e) {
            // This error means the stream ended unexpectedly. This usually happens when the last
            // packet in a LoRa payload is truncated. The rest of the stream is unrecoverable.
            // We consume all remaining bytes to terminate the parsing loop for this payload.
            String errorMsg = String.format("TelemetryParser: Truncated packet '%s' (EOF). Discarding rest of LoRa payload (%d bytes).",
                                            matchedEntry.name, loraPayload.length);
            System.out.println("[Telemetry Parser] " + errorMsg);
            notifications.post(NotificationPanel.Status.WARNING, NotificationPanel.Channel.TELEMETRY, errorMsg);
            return new TelemetryParserLUT.ParseResult(null, loraPayload.length);

        } catch (NoSuchElementException | IllegalArgumentException e) {
            // These errors mean the packet header was valid, but the content was malformed.
            // The best recovery is to skip the entire expected size of the packet that failed.
            String errorMsg = String.format("TelemetryParser: Malformed packet '%s' due to '%s'.",
                                            matchedEntry.name, e.getMessage());
            System.err.println("[Telemetry Parser] " + errorMsg);
            notifications.post(NotificationPanel.Status.WARNING, NotificationPanel.Channel.TELEMETRY, errorMsg);

            int bytesToSkip = 1;
            try {
                TelemetryRecords.CANFrame frameInfo = lookup.getFrame(Constants.specialIDs.telemetryID, matchedEntry.packetIndex);
                int totalBits = matchedEntry.bits;
                for (int i = 0; i < frameInfo.numData(); i++) {
                    totalBits += lookup.getDataInfo(Constants.specialIDs.telemetryID, matchedEntry.packetIndex, i).bitLength();
                }
                int calculatedBytes = (totalBits + 7) / 8;
                bytesToSkip = Math.min(calculatedBytes, loraPayload.length);
                notifications.post(NotificationPanel.Status.OK, NotificationPanel.Channel.TELEMETRY, "Attempting to skip " + bytesToSkip + " bytes for corrupted packet '" + matchedEntry.name + "'.");
            } catch (Exception lookupEx) {
                notifications.post(NotificationPanel.Status.WARNING, NotificationPanel.Channel.TELEMETRY, "Could not calculate size for '" + matchedEntry.name + "', skipping 1 byte.");
            }
            return new TelemetryParserLUT.ParseResult(null, bytesToSkip);
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