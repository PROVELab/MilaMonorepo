package presentation;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.PreparedStatement;
import java.io.EOFException;
import java.util.NoSuchElementException;
import java.util.Optional;

import util.Constants;
import lookup.TelemetryRecords;
import application.UI.MainFrame;
import application.UI.MainPanel;
import application.UI.NotificationPanel;
import application.UI.SensorSelectionPanel;
import lookup.TelemetryLookup;
import presentation.TelemetryParserLUT.LutEntry;
import presentation.TelemetryParserLUT.PacketVisitor;
import presentation.TelemetryParserLUT.ParsedPacket;


import java.sql.SQLException;
import java.util.UUID;
import java.util.concurrent.atomic.AtomicLong;

import javax.swing.SwingUtilities;
import javax.swing.UIManager;

import com.formdev.flatlaf.FlatDarkLaf;

/**
 * This is a static helper class for parsing the LoRa telemetry stream.
 * It uses the auto-generated TelemetryParserLUT.java for packet definitions and lookup tables.
 * This file is user-managed and is not overwritten by the code generator.
 */
public final class TelemetryParser {

    private static final String DATABASE_URL = "jdbc:sqlite:telem_dashboard.db";  // Database file path for telemetry readings
    private static final String SESSION_ID = UUID.randomUUID().toString();  // Identifier for the current telemetry session
    private static final long SESSION_START_NS = System.nanoTime(); // Used to measure elapsed time for parsing telemetry packets
    private static final AtomicLong NEXT_GROUP_SEQUENCE = new AtomicLong(0); // gives a unique sequence number for each group of parsed telemetry readings

    private TelemetryParser() {}

    public static TelemetryParserLUT.ParseResult parseSinglePacket(byte[] loraPayload, TelemetryLookup lookup, NotificationPanel notifications, TelemetryParserLUT.PacketVisitor visitor) {
        if (loraPayload.length == 0) {
            return new TelemetryParserLUT.ParseResult(Optional.empty(), 0); // No payload, consume 0 bytes
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
            return new TelemetryParserLUT.ParseResult(Optional.empty(), loraPayload.length);
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
            return new TelemetryParserLUT.ParseResult(Optional.empty(), 1);
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
            int bitsBeforeVisitor = stream.position();
            packet.accept(visitor, stream);
            if (stream.position() != bitsBeforeVisitor) {
                stream.alignToByte();
            }

            // To correctly account for packets that are not a multiple of 8 bits long,
            // we calculate consumed bytes by rounding up the number of bits consumed.
            int bitsConsumed = stream.position() - startPosBits;
            int bytesConsumed = (bitsConsumed + 7) / 8;

            return new TelemetryParserLUT.ParseResult(Optional.of(packet), bytesConsumed);

        } catch (EOFException e) {
            // This error means the stream ended unexpectedly. This usually happens when the last
            // packet in a LoRa payload is truncated. The rest of the stream is unrecoverable.
            // We consume all remaining bytes to terminate the parsing loop for this payload.
            String errorMsg = String.format("TelemetryParser: Truncated packet '%s' (EOF). Discarding rest of LoRa payload (%d bytes).",
                                            matchedEntry.name, loraPayload.length);
            System.out.println("[Telemetry Parser] " + errorMsg);
            notifications.post(NotificationPanel.Status.WARNING, NotificationPanel.Channel.TELEMETRY, errorMsg);
            return new TelemetryParserLUT.ParseResult(Optional.empty(), loraPayload.length);

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
            return new TelemetryParserLUT.ParseResult(Optional.empty(), bytesToSkip);
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
        // Computer-side parsing times
        long parsedAtMs = System.currentTimeMillis();
        long elapsedNs = System.nanoTime() - SESSION_START_NS;
        long groupSequence = NEXT_GROUP_SEQUENCE.getAndIncrement();
        
    
        try (Connection db =
                  DriverManager.getConnection(DATABASE_URL)) // Establish a connection to the database
                  {
                    try (var createTable = db.createStatement())  //create the telemetry_readings table if it doesn't exist
                    {
                        createTable.executeUpdate(""" 
                            CREATE TABLE IF NOT EXISTS telemetry_readings (
                            session_id TEXT NOT NULL,
                            group_sequence INTEGER NOT NULL,
                            parsed_at_ms INTEGER NOT NULL,
                            elapsed_ns INTEGER NOT NULL,
                            node_id INTEGER NOT NULL,
                            frame_index INTEGER NOT NULL,
                            data_index INTEGER NOT NULL,
                            value INTEGER NOT NULL,
                            PRIMARY KEY (
                                session_id,
                                group_sequence,
                                data_index
                                        )
                        
                                                                            )
                        """);
                        createTable.executeUpdate("""
                            CREATE INDEX IF NOT EXISTS idx_telemetry_session_time
                            ON telemetry_readings (session_id, parsed_at_ms)
                        """);
                    
                  }
                  db.setAutoCommit(false);  //make sure it doesn't automatically commit each statement, so you can manage transactions manually
                  try {
                        String sql = """
                            INSERT INTO telemetry_readings (
                                session_id,
                                group_sequence,
                                parsed_at_ms,
                                elapsed_ns,
                                node_id,
                                frame_index,
                                data_index,
                                value
                            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?) //placeholders for the values to be inserted
                        """;
                        try (PreparedStatement statement = db.prepareStatement(sql)) {
                            for (int i = 0; i < parsedValues.length; i++) {
                                    if (lookup.getDataInfo(nodeId, frameIndex, i).bitLength() == 0) 
                                        {
                                            continue; // Skip fields with 0 bit length
                                        } 
                                    statement.setString(1, SESSION_ID);
                                    statement.setLong(2, groupSequence);
                                    statement.setLong(3, parsedAtMs);                                                                                                                                                                                                                                                                                                       
                                    statement.setLong(4, elapsedNs);
                                    statement.setInt(5, nodeId);
                                    statement.setInt(6, frameIndex);
                                    statement.setInt(7, i);
                                    statement.setInt(8, parsedValues[i]);
                                    statement.executeUpdate();
                        
                            }
                            
                        }
                        db.commit();
                    } catch (SQLException | RuntimeException error){
                        try{
                            db.rollback();
                        } catch (SQLException rollbackError) {
                            error.addSuppressed(rollbackError);
                        }
                        throw error;
                    }
                } catch (SQLException | RuntimeException error){
                    // A logging failure doesn't discard the decoded measurements.
                    {
                        System.err.println("Telemetry logging failed: " + error.getMessage());
                  }
                }
        return parsedValues;
    }
    // Temporary visual test. Run using the testTelemetryLogger Gradle task.
public static void main(String[] args) throws Exception {

    // Load the dashboard's sensor definitions.
    TelemetryLookup lookup;

    try (var input =
                 TelemetryParser.class.getResourceAsStream("/telemetry.csv")) {

        if (input == null) {
            throw new IllegalStateException("Could not find telemetry.csv");
        }

        lookup = new TelemetryLookup(input);
    }

    // Five groups, each containing four sensor measurements.
    int[][] expected = {
        {75, 90, 78, 45},
        {78, 94, 79, 47},
        {82, 98, 81, 50},
        {79, 95, 80, 48},
        {76, 91, 78, 46}
    };

    // Encoded versions of the values above.
    // These match node 9, frame 0 in the original telemetry.csv.
    byte[][] payloads = {
        {(byte) 0xAF, (byte) 0xF8, 0x42, 0x16, 0x64},
        {(byte) 0xB2, 0x08, 0x63, 0x16, 0x66},
        {(byte) 0xB6, 0x18, (byte) 0xA3, 0x16, 0x69},
        {(byte) 0xB3, 0x0C, (byte) 0x83, 0x16, 0x67},
        {(byte) 0xB0, (byte) 0xFC, 0x42, 0x16, 0x65}
    };

    System.out.println("Recording five groups of fake telemetry...");

    for (int group = 0; group < payloads.length; group++) {

        // Space the groups approximately half a second apart.
        // The dashboard has not opened yet, so this does not freeze it.
        if (group > 0) {
            Thread.sleep(500);
        }

        BitStream stream = new BitStream(
                ByteBuffer.wrap(payloads[group])
                        .order(ByteOrder.LITTLE_ENDIAN));

        // Run the real decoding and database logging code.
        int[] decoded = parseFields(
                stream, lookup, 9, 0, 4, null);

        if (!java.util.Arrays.equals(decoded, expected[group])) {
            throw new IllegalStateException(
                    "Unexpected decoded values in group " + group
                    + ": " + java.util.Arrays.toString(decoded));
        }
    }

    // These arrays will be filled from SQL query results.
    // First index = sensor; second index = recorded group.
    double[][] recordedSeconds = new double[4][expected.length];
    int[][] recordedValues = new int[4][expected.length];

    String sql = """
        SELECT group_sequence, node_id, frame_index,
               data_index, elapsed_ns, value
        FROM telemetry_readings
        WHERE session_id = ?
        ORDER BY group_sequence, data_index
        """;

    int rows = 0;
    int expectedRows = expected.length * 4;
    long firstElapsedNs = 0;

    // The logger has closed its connections.
    // Read the committed data using a new connection.
    try (Connection db = DriverManager.getConnection(DATABASE_URL);
         PreparedStatement statement = db.prepareStatement(sql)) {

        // Ignore recordings from previous test runs.
        statement.setString(1, SESSION_ID);

        try (var results = statement.executeQuery()) {
            while (results.next()) {

                int group = rows / 4;
                int dataIndex = rows % 4;

                // Check that every saved row matches what we recorded.
                if (rows >= expectedRows
                        || results.getLong("group_sequence") != group
                        || results.getInt("node_id") != 9
                        || results.getInt("frame_index") != 0
                        || results.getInt("data_index") != dataIndex
                        || results.getInt("value")
                                != expected[group][dataIndex]) {

                    throw new IllegalStateException(
                            "Unexpected database row at position " + rows);
                }

                long elapsedNs = results.getLong("elapsed_ns");

                if (rows == 0) {
                    firstElapsedNs = elapsedNs;
                }

                // Make the first recorded group appear at 0 seconds.
                recordedSeconds[dataIndex][group] =
                        (elapsedNs - firstElapsedNs) / 1_000_000_000.0;

                recordedValues[dataIndex][group] =
                        results.getInt("value");

                rows++;
            }
        }
    }

    if (rows != expectedRows) {
        throw new IllegalStateException(
                "Expected " + expectedRows
                + " saved measurements, but found " + rows);
    }

    System.out.println("PASS: verified all 20 measurements in SQLite.");
    System.out.println("Opening the recorded-data dashboard...");

    UIManager.setLookAndFeel(new FlatDarkLaf());

    // Swing interface updates belong on Swing's UI thread.
    SwingUtilities.invokeLater(() -> {

        NotificationPanel notifications = new NotificationPanel();
        SensorSelectionPanel selectionPanel =
                new SensorSelectionPanel(lookup);

        // Use the existing dashboard with a 2-by-2 chart grid.
        MainPanel mainPanel = new MainPanel(lookup, 2, 2);

        MainFrame frame = new MainFrame(
                lookup, selectionPanel, notifications, mainPanel);

        mainPanel.connectFrame(frame);
        frame.setTitle("Telemetry Dashboard - SQLite test snapshot");

        for (int dataIndex = 0; dataIndex < 4; dataIndex++) {
            TelemetryLookup.DataKey key =
                    new TelemetryLookup.DataKey(9, 0, dataIndex);

            mainPanel.showRecordedSeries(
                    dataIndex,
                    key,
                    recordedSeconds[dataIndex],
                    recordedValues[dataIndex]);
        }

        frame.setVisible(true);
    });
}
}
