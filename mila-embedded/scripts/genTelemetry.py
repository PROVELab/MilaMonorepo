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

def createTelemetryRecords(dataPoint_fields, CANFrame_fields, records_path):
    """
    Generates TelemetryRecords.java with record definitions based on
    fields marked for 'telemetry' in parseFile.py.
    """
    os.makedirs(os.path.dirname(records_path), exist_ok=True)
    records_path = os.path.join(records_path, 'TelemetryRecords.java') # Ensure records_path is correct

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
        f.write("        String nodeName,\n")
        f.write("        int numFrames\n")
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
        f.write("import java.util.Optional;\n")
        f.write("import java.util.Comparator;\n")
        f.write("import java.util.List;\n")

        f.write("public final class TelemetryParserLUT {\n\n")
        f.write("    private TelemetryParserLUT() {}\n\n")

        f.write("    public interface PacketVisitor {\n")
        for msg in vitals_to_telem:
            is_custom = msg.get("byteCount") is CUSTOM
            return_type = "int" if is_custom else "void"
            extra_params = ", BitStream stream" if is_custom else ""
            f.write(f"        {return_type} visit({msg['name']}Packet p{extra_params});\n")
        f.write("        void visit(ParsedPacket p); // Fallback for unhandled packets\n")
        f.write("    }\n\n")
        f.write("    public static abstract class ParsedPacket {\n") # Moved to TelemetryParserLUT
        f.write("        public final String packetName;\n") # Moved to TelemetryParserLUT
        f.write("        public int packetIndex;\n") # Moved to TelemetryParserLUT
        f.write("        protected ParsedPacket(String name) { this.packetName = name; }\n") # Moved to TelemetryParserLUT
        f.write("        public abstract int accept(PacketVisitor visitor, BitStream stream);\n") # Moved to TelemetryParserLUT
        f.write("        public abstract int[] getValues();\n") # Moved to TelemetryParserLUT
        f.write("        @Override\n") # Moved to TelemetryParserLUT
        f.write("        public String toString() { return packetName; }\n") # Moved to TelemetryParserLUT
        f.write("    }\n\n") # Moved to TelemetryParserLUT
        f.write("    public static final class ParseResult {\n")
        f.write("        public final ParsedPacket packet;\n")
        f.write("        public final int bytesConsumed;\n")
        f.write("        public ParseResult(ParsedPacket p, int b) { packet = p; bytesConsumed = b; }\n")
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
            if msg.get("byteCount") is CUSTOM:
                f.write("        @Override\n        public int accept(PacketVisitor visitor, BitStream stream) { return visitor.visit(this, stream); }\n")
            else:
                f.write("        @Override\n        public int accept(PacketVisitor visitor, BitStream stream) { visitor.visit(this); return 0; }\n")
            f.write("    }\n\n")
        f.write("    static class LutEntry {\n")
        f.write("        public final int mask, bits, packetIndex;\n        public final String name;\n        public final boolean isCustom;\n        public final PacketCreator creator;\n")
        f.write("        public LutEntry(int m, int b, String n, boolean c, int pIdx) { mask=m; bits=b; name=n; isCustom=c; packetIndex=pIdx; }\n    }\n\n")
        f.write("    static final List<LutEntry> LUT = new ArrayList<>();\n\n")
        f.write("    static {\n")
        for msg in vitals_to_telem:
            name = msg["name"]
            mask = msg["mask"]
            mask_bits = msg["mask_bits"]
            packet_idx = msg['packet_idx']
            is_custom = "true" if msg.get("byteCount") is CUSTOM else "false"
            f.write(f"        LUT.add(new LutEntry(0x{mask:X}, {mask_bits}, \"{name}\", {is_custom}, {packet_idx}, {name}Packet::new));\n")
        f.write("        LUT.sort(Comparator.comparingInt(e -> e.bits)); // Sort ascending by mask length for prefix matching\n")
        f.write("    }\n\n")
        f.write("    @FunctionalInterface\n")
        f.write("    interface PacketCreator { TelemetryParserLUT.ParsedPacket create(); }\n\n")

        f.write("}\n")

def createTelemetryParserLUT(vitals_to_telem, parser_path):
    """
    Generates a Java class responsible for parsing the LoRa telemetry stream.
    This file contains all auto-generated packet definitions and lookup tables.
    """
    os.makedirs(os.path.dirname(parser_path), exist_ok=True)
    parser_path = os.path.join(parser_path, 'TelemetryParserLUT.java') # Ensure parser_path is correct

    with open(parser_path, "w") as f:
        # f.write(f"package {java_package_name};\n\n")
        f.write("import java.nio.ByteBuffer;\n")
        f.write("import java.nio.ByteOrder;\n")
        f.write("import java.util.ArrayList;\n")
        f.write("import java.util.Optional;\n")
        f.write("import java.util.Comparator;\n")
        f.write("import java.util.List;\n")

        f.write("public final class TelemetryParserLUT { // Auto-generated. Do not edit.\n\n")
        f.write("    private TelemetryParserLUT() {} // Private constructor to prevent instantiation\n\n")
        
        f.write("    public interface PacketVisitor {\n")
        for msg in vitals_to_telem:
            is_custom = msg.get("byteCount") is CUSTOM
            return_type = "int" if is_custom else "void"
            extra_params = ", BitStream stream" if is_custom else ""
            f.write(f"        {return_type} visit({msg['name']}Packet p{extra_params});\n")
        f.write("        void visit(ParsedPacket p); // Fallback for unhandled packets\n")
        f.write("    }\n\n")
        f.write("    public static abstract class ParsedPacket {\n")
        f.write("        public final String packetName;\n")
        f.write("        public int packetIndex;\n")
        f.write("        protected ParsedPacket(String name) { this.packetName = name; }\n")
        f.write("        public abstract int accept(PacketVisitor visitor, BitStream stream);\n")
        f.write("        public abstract int[] getValues();\n")
        f.write("        @Override\n")
        f.write("        public String toString() { return packetName; }\n")
        f.write("    }\n\n")
        f.write("    public static final class ParseResult {\n")
        f.write("        public final ParsedPacket packet;\n")
        f.write("        public final int bytesConsumed;\n")
        f.write("        public ParseResult(ParsedPacket p, int b) { packet = p; bytesConsumed = b; }\n")
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
            if msg.get("byteCount") is CUSTOM:
                f.write("        @Override\n        public int accept(PacketVisitor visitor, BitStream stream) { return visitor.visit(this, stream); }\n")
            else:
                f.write("        @Override\n        public int accept(PacketVisitor visitor, BitStream stream) { visitor.visit(this); return 0; }\n")
            f.write("    }\n\n")
        f.write("    static class LutEntry {\n")
        f.write("        public final int mask, bits, packetIndex;\n        public final String name;\n        public final boolean isCustom;\n        public final PacketCreator creator;\n")
        f.write("        public LutEntry(int m, int b, String n, boolean c, int pIdx, PacketCreator creator) { mask=m; bits=b; name=n; isCustom=c; packetIndex=pIdx; this.creator = creator; }\n    }\n\n")
        f.write("    static final List<LutEntry> LUT = new ArrayList<>();\n\n")
        f.write("    static {\n")
        for msg in vitals_to_telem:
            name = msg["name"]
            mask = msg["mask"]
            mask_bits = msg["mask_bits"]
            packet_idx = msg['packet_idx']
            is_custom = "true" if msg.get("byteCount") is CUSTOM else "false"
            f.write(f"        LUT.add(new LutEntry(0x{mask:X}, {mask_bits}, \"{name}\", {is_custom}, {packet_idx}, {name}Packet::new));\n")
        f.write("        LUT.sort(Comparator.comparingInt(e -> e.bits)); // Sort ascending by mask length for prefix matching\n")
        f.write("    }\n\n")
        f.write("    @FunctionalInterface\n")
        f.write("    interface PacketCreator { TelemetryParserLUT.ParsedPacket create(); }\n\n")
        f.write("}\n")

def createCommandRecords(telem_to_vitals, globalEnums):
    """
    Generates CommandRecords.java with record definitions for all telemetry commands.
    """
    java_dir = get_telem_path()
    records_path = os.path.join(java_dir, 'java', 'CommandRecords.java')
    os.makedirs(os.path.dirname(records_path), exist_ok=True)

    with open(records_path, "w") as f:
        f.write("/** Auto-generated file. Do not edit. */\n\n")
        f.write("import java.util.ArrayList;\n")
        f.write("import java.util.HashMap;\n")
        f.write("import java.util.List;\n")
        f.write("import java.util.Map;\n\n")
        f.write("public final class CommandRecords {\n")
        f.write("    private CommandRecords() {}\n\n")

        # --- Record Definitions ---
        f.write("    public record EnumEntry(String name, int value) {}\n")
        f.write("    public record CommandField(String name, int bits, int min, int max, String enumName) {}\n")
        f.write("    public record Command(String name, int mask, int maskBits, boolean isCustom, List<CommandField> fields) {\n")
        f.write("        @Override public String toString() { return name; }\n")
        f.write("    }\n\n")

        # --- Static Data Structures ---
        f.write("    public static final Map<String, List<EnumEntry>> ENUMS = new HashMap<>();\n")
        f.write("    public static final List<Command> COMMANDS = new ArrayList<>();\n\n")

        # --- Static Initializer ---
        f.write("    static {\n")
        # Populate Enums
        for enum in globalEnums:
            f.write(f"        List<EnumEntry> {enum.enum_name}_entries = new ArrayList<>();\n")
            for entry in enum.entries:
                f.write(f"        {enum.enum_name}_entries.add(new EnumEntry(\"{entry.name}\", {entry.value_int}));\n")
            f.write(f"        ENUMS.put(\"{enum.enum_name}\", {enum.enum_name}_entries);\n\n")

        # Populate Commands
        for cmd in telem_to_vitals:
            f.write(f"        List<CommandField> {cmd['name']}_fields = new ArrayList<>();\n")
            for field in cmd.get("msgFields", []):
                final_enum_name = None
                if isinstance(field.enum, str):
                    final_enum_name = field.enum
                elif field.enum is True:
                    final_enum_name = field.name
                enum_name_str = f"\"{final_enum_name}\"" if final_enum_name else "null"
                f.write(f"        {cmd['name']}_fields.add(new CommandField(\"{field.name}\", {field.bits}, {field.min}, {field.max}, {enum_name_str}));\n")
            is_custom_str = "true" if cmd.get("byteCount") is CUSTOM else "false"
            f.write(f"        COMMANDS.add(new Command(\"{cmd['name']}\", {cmd['mask']}, {cmd['mask_bits']}, {is_custom_str}, {cmd['name']}_fields));\n\n")
        f.write("    }\n") # End static initializer
        f.write("}\n")

def createTelemetry(vitals_nodes, out_path, generated_code_dir, node_ids=None, node_names=None, num_frames_per_node=None, dataNames=None, vitals_to_telem_packets=None, global_defines=None):
    """
    Builds a denormalized telemetry CSV:
      - 1 row per data point
      - Includes all fields tagged 'telemetry' from CANFrame and dataPoint
      - Adds 'data_name' from a FLAT dataNames list (consumed in traversal order)
      - Includes numFrames for each node.
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
    if num_frames_per_node is not None:
        header.append("numFrames")
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
                        **({"numFrames": num_frames_per_node[n_idx]} if num_frames_per_node is not None else {}),
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
                                "numFrames": len(vitals_to_telem_packets),
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
