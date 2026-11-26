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
import application.UI.NotificationPanel;
import lookup.TelemetryLookup;
import presentation.TelemetryParserLUT.LutEntry;
import presentation.TelemetryParserLUT.PacketVisitor;
import presentation.TelemetryParserLUT.ParsedPacket;


import java.sql.SQLException;
import java.util.UUID;
import java.util.concurrent.atomic.AtomicLong;



/**
 * This is a static helper class for parsing the LoRa telemetry stream.
 * It uses the auto-generated TelemetryParserLUT.java for packet definitions and lookup tables.
 * This file is user-managed and is not overwritten by the code generator.
 */
public final class TelemetryParser {

    private static final String DATABASE_URL = "jdbc:sqlite:telem_dashboard_v2.db";  // Database file path for telemetry readings
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
        long groupSequence = NEXT_GROUP_SEQUENCE.getAndIncrement();  // Get a unique sequence number for this group of parsed readings
        
        // telemetry data logging
        try (Connection db =
                  DriverManager.getConnection(DATABASE_URL)) // Establish a connection to the database
                  {
                    try (var createTable = db.createStatement())  //create the telemetry_readings table if it doesn't exist
                    {
                        createTable.executeUpdate("PRAGMA foreign_keys = ON");

                        createTable.executeUpdate(""" 
                            CREATE TABLE IF NOT EXISTS CAN_Frame_Fields (
                            frame_config_id INTEGER PRIMARY KEY,

                            session_id TEXT NOT NULL,
                            node_id INTEGER NOT NULL,
                            nodeName TEXT NOT NULL,
                            frame_index INTEGER NOT NULL,

                            nodeIndex INTEGER,
                            numData INTEGER NOT NULL,
                            dataTimeout INTEGER,
                            period INTEGER,
                            enableTelemCallback INTEGER CHECK (enableTelemCallback IN (0, 1)), telemetryDivider INTEGER,
                            UNIQUE (session_id, node_id, frame_index)
                            
                        
                                                                          )
                        """);

                        createTable.executeUpdate("""
                            CREATE TABLE IF NOT EXISTS dataPoints_Fields (
                            data_point_id INTEGER PRIMARY KEY,
                            frame_config_id INTEGER NOT NULL,

                            data_index INTEGER NOT NULL,
                            dataName TEXT NOT NULL,
                            "min" INTEGER,
                            "max" INTEGER,
                            bitLength INTEGER,
                            minWarning INTEGER,
                            maxWarning INTEGER,
                            enumVal TEXT,

                            UNIQUE (frame_config_id, data_index),
                            FOREIGN KEY (frame_config_id) REFERENCES CAN_Frame_Fields(frame_config_id)
                        )

                        """);


                        createTable.executeUpdate(""" 
                            CREATE TABLE IF NOT EXISTS critical_dataPoint_Fields (
                            data_point_id INTEGER PRIMARY KEY,
                            crit_count_max INTEGER NOT NULL,
                            minCritical INTEGER NOT NULL,
                            maxCritical INTEGER NOT NULL,
                            FOREIGN KEY (data_point_id) REFERENCES dataPoints_Fields(data_point_id)
                            
                        
                                                                            )
                        """);
                        createTable.executeUpdate(""" 
                            CREATE TABLE IF NOT EXISTS telemetry_readings (
                            reading_id INTEGER PRIMARY KEY,
                            data_point_id INTEGER NOT NULL,
                            group_sequence INTEGER NOT NULL,
                            parsed_at_ms INTEGER NOT NULL,
                            elapsed_ns INTEGER NOT NULL,
                            value INTEGER NOT NULL,
                            UNIQUE (data_point_id, group_sequence),
                            FOREIGN KEY (data_point_id) REFERENCES dataPoints_Fields(data_point_id)
                        
                                                                            )
                        """);

                        createTable.executeUpdate("""
                            CREATE INDEX IF NOT EXISTS idx_telemetry_session_time
                            ON telemetry_readings (data_point_id, parsed_at_ms)
                        """);
                        createTable.executeUpdate("""
                            CREATE VIEW IF NOT EXISTS telemetry_readings_seconds AS SELECT
                                f.session_id,
                                r.group_sequence,
                                r.parsed_at_ms,
                                r.elapsed_ns / 1000000000.0 AS time_seconds,
                                f.node_id,
                                f.nodeName,
                                f.frame_index,
                                d.data_index,
                                d.dataName,
                                r.value
                            FROM telemetry_readings AS r
                            JOIN dataPoints_Fields AS d ON r.data_point_id = d.data_point_id
                            JOIN CAN_Frame_Fields AS f ON d.frame_config_id = f.frame_config_id

                        """);
                    
                  }
                  db.setAutoCommit(false);  //make sure it doesn't automatically commit each statement, so you can manage transactions manually
                  try {
                        var frame = lookup.getFrame(nodeId, frameIndex);
                        if (parsedValues.length > frame.numData())
                        {
                            throw new SQLException("Parsed field count exceeds the frame definition");
                        }
                        long frameConfigId = 0;
                        try(PreparedStatement find = db.prepareStatement("""
                            SELECT frame_config_id FROM CAN_Frame_Fields WHERE session_id = ? AND node_id = ? AND frame_index = ? """))
                            {
                                find.setString(1, SESSION_ID);
                                find.setInt(2, nodeId);
                                find.setInt(3, frameIndex);
                                try(var rows = find.executeQuery())
                                {
                                    if (rows.next())
                                    {
                                        frameConfigId = rows.getLong("frame_config_id");
                                    }
                                }
                            }
                        if (frameConfigId ==0)
                        {
                            try (PreparedStatement insertFrame = db.prepareStatement("""
                                INSERT INTO CAN_Frame_Fields (session_id, node_id, nodeName, frame_index, nodeIndex, numData, dataTimeout, enableTelemCallback)
                                VALUES (?, ?, ?, ?, ?, ?, ?, ?) RETURNING frame_config_id""")) {
                                insertFrame.setString(1, SESSION_ID);
                                insertFrame.setInt(2, nodeId);
                                insertFrame.setString(3, lookup.getNodeName(nodeId));
                                insertFrame.setInt(4, frameIndex);
                                insertFrame.setInt(5, frame.nodeIndex());
                                insertFrame.setInt(6, frame.numData());
                                insertFrame.setInt(7, frame.dataTimeout());
                                insertFrame.setInt(8, frame.enableTelemCallback() ? 1 : 0);
                        
                                try (var row = insertFrame.executeQuery()) {
                                    if (!row.next()) {
                                        throw new SQLException("Frame insert returned no ID");
                                    } 
                                    frameConfigId = row.getLong("frame_config_id");
                                }
                            }
                        
                            String dataSql = """
                                INSERT INTO critical_dataPoint_Fields (frame_config_id, data_index, dataName, "min", "max", bitLength, minWarning, maxWarning, enumVal,dataName)
                                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
                                RETURNING data_point_id
                            """;
                            String criticalSql = """
                                INSERT INTO critical_dataPoint_Fields (data_point_id, crit_count_max, minCritical, maxCritical)
                                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
                                RETURNING data_point_id
                            """;
                            try (PreparedStatement insertData = db.prepareStatement(dataSql); PreparedStatement insertCritical = db.prepareStatement(criticalSql))
                            {
                                for (int i=0; i < frame.numData(); i++)
                                {
                                    var info = lookup.getDataInfo(nodeId, frameIndex, i);
                                    insertData.setLong(1, frameConfigId);
                                    insertData.setInt(2, i);
                                    insertData.setString(3, info.dataName());
                                    insertData.setInt(4, info.min());
                                    insertData.setInt(5, info.max());
                                    insertData.setInt(6, info.bitLength());
                                    insertData.setInt(7, info.minWarning());
                                    insertData.setInt(8, info.maxWarning());
                                    
                                    if (info.enumVal() == null || info.enumVal().isBlank())
                                    {
                                        insertData.setNull(9, java.sql.Types.VARCHAR);
                                    }
                                    else {
                                        insertData.setString(9, info.enumVal());
                                    }
                                    long dataPointId = 0;
                                    try (var row = insertData.executeQuery()) {
                                        if (!row.next()) {
                                            throw new SQLException("Data insert returned no ID");
                                        } 
                                        dataPointId = row.getLong("data_point_id");
                                    }
                                    insertCritical.setLong(1, dataPointId);
                                    insertCritical.setInt(2, info.crit_count_max());
                                    insertCritical.setInt(3, info.minCritical());
                                    insertCritical.setInt(4, info.maxCritical());
                                    insertCritical.executeUpdate();
                                }
                            }
                        }
                        long[] dataPointIds = new long[parsedValues.length];
                        try (PreparedStatement findData = db.prepareStatement("""
                            SELECT data_point_id,data_index FROM dataPoints_Fields WHERE frame_config_id = ? """)) 
                            {
                                findData.setLong(1, frameConfigId);
                                try (var rows = findData.executeQuery()) {
                                    while (rows.next()) {
                                        int index = rows.getInt("data_index");
                                        if (index >= 0 && index < parsedValues.length) {
                                            dataPointIds[index] = rows.getLong("data_point_id");
                                        }
                                    }
                                }
                            }

                        try (PreparedStatement statement = db.prepareStatement("""
                            INSERT INTO telemetry_readings (data_point_id, group_sequence, parsed_at_ms, elapsed_ns, value)
                            VALUES (?, ?, ?, ?, ?)""")) {
                            for (int i = 0; i < parsedValues.length; i++) {
                                if (lookup.getDataInfo(nodeId, frameIndex, i).bitLength() == 0) {
                                    continue; // Skip fields with zero bit length
                                }
                                if (dataPointIds[i] == 0) {
                                    throw new SQLException("Missing data point definition for index " + i);
                                }
                                statement.setLong(1, dataPointIds[i]);
                                statement.setLong(2, groupSequence);
                                statement.setLong(3, parsedAtMs);
                                statement.setLong(4, elapsedNs);
                                statement.setInt(5, parsedValues[i]);
                                statement.addBatch();
                            }
                            statement.executeBatch();
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
                    // A logging failure won't discard the decoded measurements.
                    {
                        System.err.println("Telemetry logging failed: " + error.getMessage());
                  }
                }
        return parsedValues;
    }
}
