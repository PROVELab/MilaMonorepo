import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.*;

import org.apache.commons.csv.CSVFormat;
import org.apache.commons.csv.CSVParser;
import org.apache.commons.csv.CSVRecord;

public class TelemetryLookup {

    /* ======================= Domain records ======================= */

    // Node as defined by CSV
    public record Node(
        int nodeId,     // CAN node ID. Primary key
        String name     // CSV column node_name 
    ) {}

    public record CANFrame(
        int frameIndex,   // index *within* node
        int dataTimeout   // CSV dataTimeout
    ) {}

    // Formerly DataPoint → now DataInfo
    public record DataInfo(
        int dataIndex,     // index *within* frame
        String dataName,   // CSV data_name
        int min,
        int max,
        int minWarning,
        int maxWarning,
        int minCritical,
        int maxCritical
    ) {}

    // Tuple keys (identity by nodeId + indices)
    public record FrameKey(int nodeId, int frameIndex) {}
    public record DataKey(int nodeId, int frameIndex, int dataIndex) {}

    // What you called a “Commitment”: the joined triple
    public record Commitment(Node node, CANFrame frame, DataInfo data) {}

    /* ======================== Normalized state ======================== */

    // nodeId → Node
    private final Map<Integer, Node> nodesById = new HashMap<>();

    // (nodeId, frameIndex) → CANFrame
    private final Map<FrameKey, CANFrame> framesById = new HashMap<>();

    // (nodeId, frameIndex, dataIndex) → DataInfo
    private final Map<DataKey, DataInfo> dataById = new HashMap<>();

    /* =========================== Construction =========================== */

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
                .build()
                .parse(r)) {

            for (CSVRecord rec : parser) {
                // Parse IDs/indices from CSV
                int nodeId   = Integer.parseInt(rec.get("nodeID"));          // primary node identity
                int frameIdx = Integer.parseInt(rec.get("frame_index"));     // index within node
                int dataIdx  = Integer.parseInt(rec.get("data_point_index"));// index within frame
                String nodeName = rec.get("node_name");

                // Upsert Node keyed by nodeId
                nodesById.putIfAbsent(nodeId, new Node(nodeId, nodeName));

                // Upsert Frame keyed by (nodeId, frameIndex)
                FrameKey fk = new FrameKey(nodeId, frameIdx);
                framesById.putIfAbsent(fk, new CANFrame(
                        frameIdx,
                        Integer.parseInt(rec.get("dataTimeout"))
                ));

                // Insert/overwrite DataInfo keyed by (nodeId, frameIndex, dataIndex)
                DataKey dk = new DataKey(nodeId, frameIdx, dataIdx);
                dataById.put(dk, new DataInfo(
                        dataIdx,
                        rec.get("data_name"),
                        Integer.parseInt(rec.get("min")),
                        Integer.parseInt(rec.get("max")),
                        Integer.parseInt(rec.get("minWarning")),
                        Integer.parseInt(rec.get("maxWarning")),
                        Integer.parseInt(rec.get("minCritical")),
                        Integer.parseInt(rec.get("maxCritical"))
                ));
            }
        }
    }

    /* ============================== Lookups ============================== */

    public Optional<Node> getNodeById(int nodeId) {
        return Optional.ofNullable(nodesById.get(nodeId));
    }

    public Optional<CANFrame> getFrameById(int nodeId, int frameIndex) {
        return Optional.ofNullable(framesById.get(new FrameKey(nodeId, frameIndex)));
    }

    public Optional<DataInfo> getDataInfoById(int nodeId, int frameIndex, int dataIndex) {
        return Optional.ofNullable(dataById.get(new DataKey(nodeId, frameIndex, dataIndex)));
    }

    public Optional<DataInfo> getDataInfoById(DataKey key) {
        return Optional.ofNullable(dataById.get(key));
    }

    public Optional<Commitment> getCommitmentById(int nodeId, int frameIndex, int dataIndex) {
        Node n = nodesById.get(nodeId);
        CANFrame f = framesById.get(new FrameKey(nodeId, frameIndex));
        DataInfo d = dataById.get(new DataKey(nodeId, frameIndex, dataIndex));
        return (n != null && f != null && d != null)
                ? Optional.of(new Commitment(n, f, d))
                : Optional.empty();
    }

    public Optional<Commitment> getCommitmentById(DataKey key) {
        return getCommitmentById(key.nodeId(), key.frameIndex(), key.dataIndex());
    }

    /* ============================= Iteration ============================= */

    /** All data keys (useful for building LeftPanel rows, etc.). */
    public Set<DataKey> allDataKeys() {
        return Collections.unmodifiableSet(dataById.keySet());
    }

    /** Handy for chart titles, etc.: "<nodeName>.<dataName>" */
    public String titleFor(DataKey key) {
        Node n = nodesById.get(key.nodeId());
        DataInfo d = dataById.get(key);
        String nodePart = (n != null) ? n.name() : ("node" + key.nodeId());
        String dataPart = (d != null) ? d.dataName() : ("dp" + key.dataIndex());
        return nodePart + "." + dataPart;
    }

    /* ============================ Optional accessors ============================ */

    public Map<Integer, Node> nodesById() { return Collections.unmodifiableMap(nodesById); }
    public Map<FrameKey, CANFrame> framesById() { return Collections.unmodifiableMap(framesById); }
    public Map<DataKey, DataInfo> dataById() { return Collections.unmodifiableMap(dataById); }
}
