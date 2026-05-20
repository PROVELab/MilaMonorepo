import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.*;

import javax.swing.SwingUtilities;

import org.apache.commons.csv.CSVFormat;
import org.apache.commons.csv.CSVParser;
import org.apache.commons.csv.CSVRecord;

public class TelemetryLookup {

    //**  Node, CANFrame, and DataInfo are auto-generated in TelemetryRecords.java  **//

    // Tuple keys (identity by nodeId + indices)
    public record FrameKey(int nodeId, int frameIndex) {}
    public record DataKey(int nodeId, int frameIndex, int dataIndex) {}

    // Commitment for each triple (node, frame sent by that node, data within that frame)
    public record Commitment(TelemetryRecords.Node node, TelemetryRecords.CANFrame frame, TelemetryRecords.DataInfo data) {}

    /* ============== Hashmaps to look up specific items ================*/
    // nodeId → Node
    private final Map<Integer, TelemetryRecords.Node> nodesById = new HashMap<>();
    // (nodeId, frameIndex) → CANFrame
    private final Map<FrameKey, TelemetryRecords.CANFrame> framesById = new HashMap<>();
    // (nodeId, frameIndex, dataIndex) → DataInfo
    private final Map<DataKey, TelemetryRecords.DataInfo> dataById = new HashMap<>();


    /* =========================== Construction of the class from CSV =========================== */

    /** Load from a filesystem path. */
    public TelemetryLookup(String csvPath) throws IOException {
        try (Reader r = new FileReader(csvPath, StandardCharsets.UTF_8)) {
            loadInto(r);
        }
    }
    /** Load from a classpath resource InputStream (e.g., getResourceAsStream). */
    public TelemetryLookup(InputStream in) throws IOException {
        try (Reader r = new InputStreamReader(in, StandardCharsets.UTF_8)) {
            loadInto(r);
        }
    }

    private void loadInto(Reader r) throws IOException {
        try (CSVParser parser = CSVFormat.Builder.create(CSVFormat.DEFAULT)
                .setHeader()
                .setSkipHeaderRecord(true)
                .get()
                .parse(r)) {

            for (CSVRecord rec : parser) {  //for each line of telemetry.csv
                // Parse IDs/indices from CSV
                int nodeId   = Integer.parseInt(rec.get("nodeID"));          // primary node identity
                int frameIdx = Integer.parseInt(rec.get("frameIndex"));     // index within node
                int dataIdx  = Integer.parseInt(rec.get("dataIndex"));// index within frame

                // Insert new node if needed
                nodesById.putIfAbsent(
                    nodeId,
                    RecordFactory.createRecord(TelemetryRecords.Node.class, rec::get, Map.of("nodeId", nodeId))
                );

                // Insert new frame if needed
                FrameKey fk = new FrameKey(nodeId, frameIdx);
                framesById.putIfAbsent(
                    fk,
                    RecordFactory.createRecord(TelemetryRecords.CANFrame.class, rec::get, Map.of("frameIndex", frameIdx))
                );

                // Insert new if needed DataInfo
                DataKey dk = new DataKey(nodeId, frameIdx, dataIdx);
                dataById.put(
                    dk,
                    RecordFactory.createRecord(TelemetryRecords.DataInfo.class, rec::get, Map.of("dataIndex", dataIdx))
                );
            }
        }
    }

    /* ============================== Public Lookup Functions ============================== */
    public Optional<TelemetryRecords.Node> getNodeById(int nodeId) {
        return Optional.ofNullable(nodesById.get(nodeId));
    }

    public Optional<TelemetryRecords.CANFrame> getFrame(int nodeId, int frameIndex) {
        return Optional.ofNullable(framesById.get(new FrameKey(nodeId, frameIndex)));
    }

    public Optional<TelemetryRecords.DataInfo> getDataInfo(int nodeId, int frameIndex, int dataIndex) {
        return Optional.ofNullable(dataById.get(new DataKey(nodeId, frameIndex, dataIndex)));
    }

    public Optional<TelemetryRecords.DataInfo> getDataInfo(DataKey key) {
        return Optional.ofNullable(dataById.get(key));
    }

    public Optional<Commitment> getCommitmentById(int nodeId, int frameIndex, int dataIndex) {
        TelemetryRecords.Node n = nodesById.get(nodeId);
        TelemetryRecords.CANFrame f = framesById.get(new FrameKey(nodeId, frameIndex));
        TelemetryRecords.DataInfo d = dataById.get(new DataKey(nodeId, frameIndex, dataIndex));
        return (n != null && f != null && d != null)
                ? Optional.of(new Commitment(n, f, d))
                : Optional.empty();
    }

    public Optional<Commitment> getCommitmentById(DataKey key) {    //Optional give as dataKey instead of each component
        return getCommitmentById(key.nodeId(), key.frameIndex(), key.dataIndex());
    }

    /* ========= Convenient String Helpers ======= */

    /** Handy for chart titles, etc.: "<nodeName>.<dataName>" */
    public String titleFor(DataKey key) {
        TelemetryRecords.Node n = nodesById.get(key.nodeId());
        TelemetryRecords.DataInfo d = dataById.get(key);
        String nodePart = (n != null) ? n.nodeName() : ("node" + key.nodeId());
        String dataPart = (d != null) ? d.dataName() : ("dp" + key.dataIndex());
        return nodePart + "." + dataPart;
    }

    public Optional<String> getNodeName(int nodeId){
        //return name of sensor, if this is a sensors
        Optional<TelemetryRecords.Node> sensorIDOpt = getNodeById(nodeId);
        if(sensorIDOpt.isPresent()){
            return Optional.of(sensorIDOpt.get().nodeName());
        }

        //Otherwise, check special IDs:
        return IntConstUtils.nameFromInt(Constants.specialIDs.class, nodeId);
    }

    /* ============================= Iteration ============================= */

    /** All data keys (useful for building LeftPanel rows, etc.). */
    public Set<DataKey> allDataKeys() {
        return Collections.unmodifiableSet(dataById.keySet());
    }

    public Set<Integer> allNodeIDs(){
        return Collections.unmodifiableSet(nodesById.keySet());
    }

    /* ============================ Acces entire map ============================ */
    public Map<Integer, TelemetryRecords.Node> nodesById() { return Collections.unmodifiableMap(nodesById); }
    public Map<FrameKey, TelemetryRecords.CANFrame> framesById() { return Collections.unmodifiableMap(framesById); }
    public Map<DataKey, TelemetryRecords.DataInfo> dataById() { return Collections.unmodifiableMap(dataById); }
}
