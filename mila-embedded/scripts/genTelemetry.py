import os
import csv
from parseFile import ACCESS, dataPoint_fields, CANFrame_fields, globalDefines, expression_to_int
from packetFormat import CUSTOM, msgField

java_package_name = "MILA.V2"
def get_telem_path():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    # Go up two levels (../..) -> into telem-dashboard -> src -> main -> java
    java_dir = os.path.join(script_dir, "..", "..", "telem-dashboard", "src", "main")
    java_dir = os.path.normpath(java_dir)
    return java_dir

def createTelemetryRecords(dataPoint_fields, CANFrame_fields):
    """
    Generates TelemetryRecords.java with record definitions based on
    fields marked for 'telemetry' in parseFile.py.
    """
    java_dir = get_telem_path()
    records_path = os.path.join(java_dir, 'java', 'TelemetryRecords.java')
    os.makedirs(os.path.dirname(records_path), exist_ok=True)

    def to_java_type(c_type):
        if 'int' in c_type:
            return 'int'
        if 'str' in c_type:
            return 'String'
        if c_type == 'boolean':
            return 'boolean'
        return 'Object' # Fallback

    with open(records_path, "w") as f:
        f.write("/** Auto-generated file. Do not edit. */\n\n")
        f.write("public final class TelemetryRecords {\n")
        f.write("    private TelemetryRecords() {}\n\n")

        # --- Node Record ---
        f.write("    public record Node(\n")
        f.write("        int nodeID,\n")
        f.write("        String nodeName\n")
        f.write("    ) {}\n\n")

        # --- CANFrame Record ---
        f.write("    public record CANFrame(\n")
        f.write("        int frameIndex,\n") # Special case
        frame_telemetry_fields = [field for field in CANFrame_fields if "telemetry" in field.get("node", [])]
        for i, field in enumerate(frame_telemetry_fields):
            f.write(f"        {to_java_type(field['type'])} {field['name']}")
            if i < len(frame_telemetry_fields) - 1:
                f.write(",")
            f.write("\n")
        f.write("    ) {}\n\n")

        # --- DataInfo Record ---
        f.write("    public record DataInfo(\n")
        f.write("        int dataIndex,\n") # Special case
        f.write("        String dataName,\n") # Special case
        dp_telemetry_fields = [field for field in dataPoint_fields if "telemetry" in field.get("node", [])]
        for i, field in enumerate(dp_telemetry_fields):
            py_name = field['name']
            java_name = 'bitLength' if py_name == 'bits' else ('enumVal' if py_name == 'enum' else py_name)
            f.write(f"        {to_java_type(field['type'])} {java_name}")
            if i < len(dp_telemetry_fields) - 1:
                f.write(",")
            f.write("\n")
        f.write("    ) {}\n\n")

        f.write("}\n")

def createTelemetryParser(vitals_to_telem, globalEnums):
    """
    Generates a Java class responsible for parsing the LoRa telemetry stream.
    """
    java_dir = get_telem_path()
    parser_path = os.path.join(java_dir, 'java', 'TelemetryParser.java')
    os.makedirs(os.path.dirname(parser_path), exist_ok=True)

    with open(parser_path, "w") as f:
        # f.write(f"package {java_package_name};\n\n")
        f.write("import java.nio.ByteBuffer;\n")
        f.write("import java.nio.ByteOrder;\n")
        f.write("import java.util.ArrayList;\n")
        f.write("import java.util.Comparator;\n")
        f.write("import java.util.List;\n")

        f.write("\npublic final class TelemetryParser {\n\n")
        f.write("    private TelemetryParser() {}\n\n")

        # --- Generate Visitor for dispatch ---
        f.write("    public interface PacketVisitor {\n")
        for msg in vitals_to_telem:
            f.write(f"        void visit({msg['name']}Packet p);\n")
        f.write("        void visit(ParsedPacket p); // Fallback for unhandled packets\n")
        f.write("    }\n\n")

        # --- Generate Packet Data Classes ---
        f.write("    public static abstract class ParsedPacket {\n")
        f.write("        public final String packetName;\n")
        f.write("        public int packetIndex;\n")
        f.write("        protected ParsedPacket(String name) { this.packetName = name; }\n")
        f.write("        public abstract void accept(PacketVisitor visitor);\n")
        f.write("        public abstract int[] getValues();\n")
        f.write("        @Override\n")
        f.write("        public String toString() { return packetName; }\n")
        f.write("    }\n\n")

        for msg in vitals_to_telem:
            name = msg["name"]
            fields = msg.get("msgFields", [])
            num_fields = len(fields)
            class_name = f"{name}Packet"
            f.write(f"    public static class {class_name} extends ParsedPacket {{\n")
            f.write(f"        public final int[] values = new int[{num_fields}];\n")
            if msg.get("byteCount") is CUSTOM:
                f.write("        public byte[] payload;\n")
            f.write(f"        public {class_name}() {{ super(\"{name}\"); }}\n")
            for i, field in enumerate(fields):
                f.write(f"        public int {field.name}() {{ return values[{i}]; }}\n")
            f.write("        @Override\n")
            f.write("        public int[] getValues() { return values; }\n")
            f.write("        @Override\n")
            f.write("        public void accept(PacketVisitor visitor) { visitor.visit(this); }\n")
            f.write("    }\n\n")

        # --- Generate LUT and Parser Logic ---
        f.write("    private static class LutEntry {\n")
        f.write("        public final int mask, bits, packetIndex;\n        public final String name;\n        public final boolean isCustom;\n")
        f.write("        public LutEntry(int m, int b, String n, boolean c, int pIdx) { mask=m; bits=b; name=n; isCustom=c; packetIndex=pIdx; }\n    }\n\n")
        f.write("    private static final List<LutEntry> LUT = new ArrayList<>();\n\n")
        f.write("    static {\n")
        for msg in vitals_to_telem:
            name = msg["name"]
            mask = msg["mask"]
            mask_bits = msg["mask_bits"]
            packet_idx = msg['packet_idx']
            is_custom = "true" if msg.get("byteCount") is CUSTOM else "false"
            
            f.write(f"        LUT.add(new LutEntry(0x{mask:X}, {mask_bits}, \"{name}\", {is_custom}, {packet_idx}));\n")
        f.write("        LUT.sort(Comparator.comparingInt(e -> e.bits)); // Sort ascending by mask length for prefix matching\n")
        f.write("    }\n\n")

        f.write("""
    public static List<ParsedPacket> parse(byte[] loraPayload, TelemetryLookup lookup) {
        List<ParsedPacket> packets = new ArrayList<>();
        BitStream stream = new BitStream(ByteBuffer.wrap(loraPayload).order(ByteOrder.LITTLE_ENDIAN));
        while (stream.hasRemaining()) {
            ParsedPacket packet = parseNext(stream, lookup);
            if (packet != null) {
                packets.add(packet);
            } else {
                if (stream.hasRemaining()) {
                    System.err.println("Parser failed to find matching packet, stopping.");
                }
                break;
            }
        }
        return packets;
    }

    private static ParsedPacket parseNext(BitStream stream, TelemetryLookup lookup) {
        LutEntry matchedEntry = null;
        for (LutEntry entry : LUT) {
            // All packets are required to have a mask of at least 1 bit.
            long peeked = stream.peek(entry.bits);
            if (peeked != -1 && peeked == entry.mask) {
                matchedEntry = entry;
                break;
            }
        }

        if (matchedEntry == null) {
            return null;
        }

        if (matchedEntry.bits > 0) {
            stream.read(matchedEntry.bits);
        }

        switch (matchedEntry.name) {""")
        for msg in vitals_to_telem:
            name = msg["name"]
            class_name = f"{name}Packet"
            fields = msg.get("msgFields", [])
            num_fields = len(fields)
            f.write(f"\n            case \"{name}\": {{\n")
            f.write(f"                {class_name} p = new {class_name}();\n")
            f.write(f"                p.packetIndex = matchedEntry.packetIndex;\n")

            if name == 'CANDataFrame':
                f.write(f"                p.values[0] = (int)stream.read({fields[0].bits}); // nodeID\n")
                f.write("                stream.alignToByte();\n")
                f.write("                p.payload = stream.readBytes(stream.remainingBytes());\n")
            else:
                f.write(f"                for (int i = 0; i < {num_fields}; i++) {{\n")
                f.write(f"                    java.util.Optional<TelemetryRecords.DataInfo> dataInfoOpt = lookup.getDataInfo(Constants.specialIDs.vitalsID, p.packetIndex, i);\n")
                f.write(f"                    if (dataInfoOpt.isEmpty()) {{ System.err.println(\"Parser: Missing data info for {name} field \" + i); continue; }}\n")
                f.write(f"                    TelemetryRecords.DataInfo dataInfo = dataInfoOpt.get();\n")
                f.write(f"                    if (dataInfo.bitLength() == 0) continue;\n")
                f.write(f"                    long raw_val = stream.read(dataInfo.bitLength());\n")
                f.write(f"                    if (dataInfo.min() < 0 && (dataInfo.bitLength() < 64) && (raw_val & (1L << (dataInfo.bitLength() - 1))) != 0) {{ raw_val |= -1L << dataInfo.bitLength(); }}\n")
                f.write(f"                    p.values[i] = (int)raw_val + dataInfo.min();\n")
                f.write(f"                }}\n")
                if msg.get("byteCount") is CUSTOM:
                    f.write("                stream.alignToByte();\n")
                    if name == 'vitalsErr':
                        f.write("                int numErrors = p.values[0];\n")
                        f.write("                p.payload = stream.readBytes(numErrors * 2);\n")
                    else:
                        f.write("                p.payload = stream.readBytes(stream.remainingBytes());\n")

            f.write(f"                return p;\n")
            f.write(f"            }}")
        f.write("""
            default:
                return null;
        }
    }\n""")

        f.write("}\n")

# Created by Chat. dont bother reading, it makes the appropriate CSV for telem dashboard.
def createTelemetry(vitals_nodes, out_path, generated_code_dir, node_ids=None, node_names=None, dataNames=None, vitals_to_telem_packets=None, global_defines=None):
    """
    Builds a denormalized telemetry CSV:
      - 1 row per data point
      - Includes all fields tagged 'telemetry' from CANFrame and dataPoint
      - Adds 'data_name' from a FLAT dataNames list (consumed in traversal order)
    """
    if dataNames is None:
        dataNames = []
    if vitals_to_telem_packets is None:
        vitals_to_telem_packets = []
    flat_idx = 0  # consume from dataNames in order

    def get(entity, key):
        return ACCESS(entity, key)["value"]

    dp_fields = [f["name"] for f in dataPoint_fields if "telemetry" in f.get("node", [])]
    frame_fields = [f["name"] for f in CANFrame_fields if "telemetry" in f.get("node", [])]

    # Map python field names to java record field names for the CSV header
    dp_field_map = {'bits': 'bitLength', 'enum': 'enumVal'}

    # Create header with Java names
    header = ["nodeID", "frameIndex", "dataIndex"]
    if node_names is not None:
        header.append("nodeName")
    header.append("dataName")
    header += frame_fields
    header += [dp_field_map.get(f, f) for f in dp_fields]

    csv_path = os.path.join(get_telem_path(), 'resources', 'telemetry.csv')

    with open(csv_path, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=header)
        w.writeheader()

        for n_idx, node in enumerate(vitals_nodes):
            frames = get(node, "CANFrames")
            for f_idx, frame in enumerate(frames):
                frame_vals = {k: get(frame, k) for k in frame_fields}
                dps = get(frame, "dataInfo")
                for d_idx, dp in enumerate(dps):
                    # pull next name (or empty if we ran out)
                    data_name = dataNames[flat_idx] if flat_idx < len(dataNames) else ""
                    flat_idx += 1

                    dp_vals = {}
                    for k in dp_fields:
                        java_key = dp_field_map.get(k, k)
                        dp_vals[java_key] = get(dp, k)

                    row = {
                        "nodeID": node_ids[n_idx] if node_ids is not None else n_idx,
                        "frameIndex": f_idx,
                        "dataIndex": d_idx,
                        **({"nodeName": node_names[n_idx]} if node_names is not None else {}),
                        "dataName": data_name,
                        **frame_vals,
                        **dp_vals,
                    }
                    w.writerow(row)
        
        # Add plottable fields from vitals_to_telem packets
        if vitals_to_telem_packets and global_defines:
            try: 
                vitals_id = expression_to_int("specialIDs::vitalsID")
            except (ValueError, NameError):
                print("Warning: specialIDs::vitalsID not found. Cannot add plottable telemetry packet fields.")
                vitals_id = -1 # Or some other indicator

            if vitals_id != -1:
                for packet_idx, packet in enumerate(vitals_to_telem_packets):
                    packet_name = packet['name']
                    msg_fields = packet.get("msgFields", [])
                    for field_idx, field in enumerate(msg_fields):
                        if getattr(field, 'plottable', False):
                            row = {
                                "nodeID": vitals_id,
                                "frameIndex": packet_idx,
                                "dataIndex": field_idx,
                                "nodeName": "vitals",
                                "dataName": f"{packet_name}_{field.name}",
                            }

                            # Add frame-level fields from the packet definition
                            for k in frame_fields:
                                row[k] = packet.get(k, 0)

                            # Add data-point-level fields from the msgField object
                            for k in dp_fields:
                                key_for_row = dp_field_map.get(k, k)
                                val = getattr(field, k, None)

                                # Apply sensible defaults if the value isn't specified in packetFormat.py
                                if val is None:
                                    if k in ('minWarning', 'minCritical'):
                                        val = getattr(field, 'min', 0)
                                    elif k in ('maxWarning', 'maxCritical'):
                                        val = getattr(field, 'max', 0)
                                    else:
                                        # Default for startingValue, crit_count_max, flags, etc.
                                        val = 0
                                row[key_for_row] = val

                            w.writerow({k: row.get(k, "") for k in header})
