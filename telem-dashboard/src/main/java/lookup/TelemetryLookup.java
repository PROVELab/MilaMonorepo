package lookup;
import java.io.*;
import java.io.EOFException;
import java.nio.charset.StandardCharsets;
import java.util.*;

import javax.swing.SwingUtilities;

import org.apache.commons.csv.CSVFormat;
import org.apache.commons.csv.CSVParser;
import org.apache.commons.csv.CSVRecord;

import util.Constants;
import util.IntConstUtils;
import presentation.BitStream;
import lookup.TelemetryRecords.*;

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

    //Stream parsing lookup functions:
    public TelemetryRecords.CANFrame lookupFrameFromStream(BitStream payloadStream, TelemetryRecords.Node nodeInfo) throws NoSuchElementException, EOFException, IllegalArgumentException {
     // Calculate bits for frameIndex based on the number of frames for this specific node.
            if (nodeInfo.numFrames() <= 0) {
                throw new NoSuchElementException("Invalid numFrames for nodeId " + nodeInfo.nodeID() + ": " + nodeInfo.numFrames());
            }

            int bitsForFrames = (nodeInfo.numFrames() == 1) ? 1 : 32 - Integer.numberOfLeadingZeros(nodeInfo.numFrames() - 1);
            int frameIndex = payloadStream.read(bitsForFrames);

            TelemetryRecords.CANFrame frame = getFrame(nodeInfo.nodeID(), frameIndex);
            return frame;
    }

    /* ============================== Public Lookup Functions ============================== */
    public Optional<TelemetryRecords.Node> getNodeByIdOpt(int nodeId) {
        return Optional.ofNullable(nodesById.get(nodeId));
    }

    public TelemetryRecords.Node getNodeById(int nodeId) throws NoSuchElementException {
        return getNodeByIdOpt(nodeId).orElseThrow(() 
            -> new NoSuchElementException("Node not found for ID: " + nodeId));
    }

    public Optional<TelemetryRecords.CANFrame> getFrameOpt(int nodeId, int frameIndex) {
        return Optional.ofNullable(framesById.get(new FrameKey(nodeId, frameIndex)));
    }

    public TelemetryRecords.CANFrame getFrame(int nodeId, int frameIndex) throws NoSuchElementException {
        return getFrameOpt(nodeId, frameIndex).orElseThrow(()
            -> new NoSuchElementException("CANFrame not found for (nodeId=" + nodeId + ", frameIndex=" + frameIndex + ")"));
    }

    public Optional<TelemetryRecords.DataInfo> getDataInfoOpt(int nodeId, int frameIndex, int dataIndex) {
        return Optional.ofNullable(dataById.get(new DataKey(nodeId, frameIndex, dataIndex)));
    }

    public TelemetryRecords.DataInfo getDataInfo(int nodeId, int frameIndex, int dataIndex) throws NoSuchElementException {
        return getDataInfoOpt(nodeId, frameIndex, dataIndex).orElseThrow(()
            -> new NoSuchElementException("DataInfo not found for (nodeId=" + nodeId + ", frameIndex=" + frameIndex + ", dataIndex=" + dataIndex + ")"));
    }

    public Optional<TelemetryRecords.DataInfo> getDataInfoOpt(DataKey key) {
        return Optional.ofNullable(dataById.get(key));
    }

    public TelemetryRecords.DataInfo getDataInfo(DataKey key) throws NoSuchElementException {
        return getDataInfoOpt(key).orElseThrow(()
            -> new NoSuchElementException("DataInfo not found for key: " + key));
    }

    public Optional<Commitment> getCommitmentByIdOpt(int nodeId, int frameIndex, int dataIndex) {
        TelemetryRecords.Node n = nodesById.get(nodeId);
        TelemetryRecords.CANFrame f = framesById.get(new FrameKey(nodeId, frameIndex));
        TelemetryRecords.DataInfo d = dataById.get(new DataKey(nodeId, frameIndex, dataIndex));
        return (n != null && f != null && d != null)
                ? Optional.of(new Commitment(n, f, d))
                : Optional.empty();
    }

    public Commitment getCommitmentById(int nodeId, int frameIndex, int dataIndex) throws NoSuchElementException {
        return getCommitmentByIdOpt(nodeId, frameIndex, dataIndex).orElseThrow(()
            -> new NoSuchElementException("Commitment not found for (nodeId=" + nodeId + ", frameIndex=" + frameIndex + ", dataIndex=" + dataIndex + ")"));
    }

    public Optional<Commitment> getCommitmentByIdOpt(DataKey key) {    //Optional give as dataKey instead of each component
        return getCommitmentByIdOpt(key.nodeId(), key.frameIndex(), key.dataIndex());
    }

    public Commitment getCommitmentById(DataKey key) throws NoSuchElementException {
        return getCommitmentByIdOpt(key).orElseThrow(()
            -> new NoSuchElementException("Commitment not found for key: " + key));
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

    public Optional<String> getNodeNameOpt(int nodeId){
        //return name of sensor, if this is a sensors
        Optional<TelemetryRecords.Node> sensorIDOpt = getNodeByIdOpt(nodeId);
        if(sensorIDOpt.isPresent()){
            return Optional.of(sensorIDOpt.get().nodeName());
        }

        //Otherwise, check special IDs:
        return IntConstUtils.nameFromInt(Constants.specialIDs.class, nodeId);
    }

    public String getNodeName(int nodeId) throws NoSuchElementException {
        return getNodeNameOpt(nodeId).orElseThrow(()
            -> new NoSuchElementException("Node name not found for ID: " + nodeId));
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
