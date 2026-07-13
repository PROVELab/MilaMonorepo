import os
import csv
from typing import Any

from config.parseFile import ACCESS, ParsedFields, expression_to_int, Node

TELEMETRY_MIN_TIMEOUT = 2000 # ms
#give at least 2000ms for lora shenanigans before raising warning timeouts on telemetries end

def get_telemetry_datapoint_fields(fields: ParsedFields) -> list[Any]:
    # Telemetry still expects the pre-split datapoint view, so include critical
    # datapoint metadata alongside the base datapoint fields.
    return [
        field
        for field in (fields.dataPoint_fields + fields.critical_dataPoint_fields)
        if "telemetry" in field.get("node", [])
    ]

def createTelemetryRecords(
    fields: ParsedFields,
    records_path: str,
) -> None:
    """
    Generates TelemetryRecords.java with record definitions based on
    fields marked for 'telemetry' in parseFile.py.
    """
    os.makedirs(os.path.dirname(records_path), exist_ok=True)

    def to_java_type(c_type: str) -> str:
        if 'int' in c_type:
            return 'int'
        if 'str' in c_type:
            return 'String'
        if c_type == 'boolean':
            return 'boolean'
        return 'Object' # Fallback

    with open(records_path, "w") as f:
        f.write("/** Auto-generated file. Do not edit. */\n\n")
        f.write("package lookup;\n\n")
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
        frame_telemetry_fields = [field for field in fields.CANFrame_fields if "telemetry" in field.get("node", [])]
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
        f.write("        boolean plottable,\n")
        dp_telemetry_fields = get_telemetry_datapoint_fields(fields)
        for i, field in enumerate(dp_telemetry_fields):
            py_name = field['name']
            java_name = 'bitLength' if py_name == 'bits' else ('enumVal' if py_name == 'enum' else py_name)
            f.write(f"        {to_java_type(field['type'])} {java_name}")
            if i < len(dp_telemetry_fields) - 1:
                f.write(",")
            f.write("\n")
        f.write("    ) {}\n\n")

        f.write("}\n")

def createTelemetryParserLUT(
    vitals_to_telem: list[dict[str, Any]],
    parser_path: str,
) -> None:
    """
    Generates a Java class responsible for parsing the LoRa telemetry stream.
    This file contains all auto-generated packet definitions and lookup tables.
    """
    os.makedirs(os.path.dirname(parser_path), exist_ok=True)

    with open(parser_path, "w") as f:
        f.write("package presentation;\n\n")
        f.write("import java.nio.ByteBuffer;\n")
        f.write("import java.nio.ByteOrder;\n")
        f.write("import java.util.ArrayList;\n")
        f.write("import java.util.Optional;\n")
        f.write("import java.util.Comparator;\n")
        f.write("import java.util.List;\n")
        f.write("import presentation.BitStream;\n")

        f.write("public final class TelemetryParserLUT { // Auto-generated. Do not edit.\n\n")
        f.write("    private TelemetryParserLUT() {} // Private constructor to prevent instantiation\n\n")
        
        f.write("    public interface PacketVisitor {\n")
        for msg in vitals_to_telem:
            is_custom = msg.get("containsPayload", False)
            return_type = "void"
            extra_params = ", BitStream stream" if is_custom else ""
            f.write(f"        {return_type} visit({msg['name']}Packet p{extra_params});\n")
        f.write("        void visit(ParsedPacket p); // Fallback for unhandled packets\n")
        f.write("    }\n\n")
        f.write("    public static abstract class ParsedPacket {\n")
        f.write("        public final String packetName;\n")
        f.write("        public int packetIndex;\n")
        f.write("        protected ParsedPacket(String name) { this.packetName = name; }\n")
        f.write("        public abstract void accept(PacketVisitor visitor, BitStream stream);\n")
        f.write("        public abstract int[] getValues();\n")
        f.write("        @Override\n")
        f.write("        public String toString() { return packetName; }\n")
        f.write("    }\n\n")
        f.write("    public static final class ParseResult {\n")
        f.write("        public final Optional<ParsedPacket> packet;\n")
        f.write("        public final int bytesConsumed;\n")
        f.write("        public ParseResult(Optional<ParsedPacket> p, int b) { packet = p; bytesConsumed = b; }\n")
        f.write("    }\n\n")
        for msg in vitals_to_telem:
            name = msg["name"]
            fields = msg.get("msgFields", [])
            num_fields = len(fields)
            class_name = f"{name}Packet"
            f.write(f"    public static class {class_name} extends ParsedPacket {{\n")
            f.write(f"        public final int[] values = new int[{num_fields}];\n")
            if msg.get("containsPayload", False):
                f.write("        public byte[] payload;\n")
            f.write(f"        public {class_name}() {{ super(\"{name}\"); }}\n")
            for i, field in enumerate(fields):
                f.write(f"        public int {field.name}() {{ return values[{i}]; }}\n")
            f.write("        @Override\n")
            f.write("        public int[] getValues() { return values; }\n")
            f.write("        @Override\n")
            f.write("        public void accept(PacketVisitor visitor, BitStream stream) {\n")
            if msg.get("containsPayload", False):
                f.write("            visitor.visit(this, stream);\n")
            else:
                f.write("            visitor.visit(this);\n")
            f.write("        }\n")
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
            is_custom = "true" if msg.get("containsPayload", False) else "false"
            f.write(f"        LUT.add(new LutEntry(0x{mask:X}, {mask_bits}, \"{name}\", {is_custom}, {packet_idx}, {name}Packet::new));\n")
        f.write("        LUT.sort(Comparator.comparingInt((LutEntry e) -> e.bits).thenComparingInt(e -> e.mask)); // Sort by length, then mask for a stable, predictable order.\n")
        f.write("    }\n\n")
        f.write("    @FunctionalInterface\n")
        f.write("    interface PacketCreator { TelemetryParserLUT.ParsedPacket create(); }\n\n")
        f.write("}\n")

def createCommandRecords(
    telem_to_vitals: list[dict[str, Any]],
    globalEnums: list[Any],
    records_path: str,
) -> None:
    """
    Generates CommandRecords.java with record definitions for all telemetry commands.
    """
    os.makedirs(os.path.dirname(records_path), exist_ok=True)

    with open(records_path, "w") as f:
        f.write("/** Auto-generated file. Do not edit. */\n\n")
        f.write("package presentation;\n\n")
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
            is_custom_str = "true" if cmd.get("containsPayload", False) else "false"
            f.write(f"        COMMANDS.add(new Command(\"{cmd['name']}\", {cmd['mask']}, {cmd['mask_bits']}, {is_custom_str}, {cmd['name']}_fields));\n\n")
        f.write("    }\n") # End static initializer
        f.write("}\n")

def genTelemCSV(nodes: list[Node],
                fields: ParsedFields,
                csv_path: str,
                vitals_to_telem_packets: list[dict[str, Any]] | None = None) -> None:
    """
    Builds a denormalized telemetry CSV:
      - 1 row per data point
      - Includes all fields tagged 'telemetry' from CANFrame and dataPoint
      - Adds 'data_name' from a FLAT dataNames list (consumed in traversal order)
      - Includes numFrames for each node.
    """
    if vitals_to_telem_packets is None:
        vitals_to_telem_packets = []
    all_data_names = [name for node in nodes for name in node.data_names]
    flat_idx = 0  # consume from dataNames in order

    def get(entity: Any, key: str) -> Any:
        return ACCESS(entity, key)["value"]

    dp_fields = [f["name"] for f in get_telemetry_datapoint_fields(fields)]
    frame_fields = [f["name"] for f in fields.CANFrame_fields if "telemetry" in f.get("node", [])]

    # Map python field names to java record field names for the CSV header
    dp_field_map = {'bits': 'bitLength', 'enum': 'enumVal'}

    # Create header with Java names
    header = ["nodeID", "frameIndex", "dataIndex"]
    header.append("nodeName")
    header.append("numFrames")
    header.append("dataName")
    header.append("plottable")
    header += frame_fields
    header += [dp_field_map.get(f, f) for f in dp_fields]

    os.makedirs(os.path.dirname(csv_path), exist_ok=True)

    with open(csv_path, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=header)
        w.writeheader()

        for node_info in nodes:
            node_vitals_data = node_info.vitals_data
            frames = get(node_vitals_data, "CANFrames")
            for f_idx, frame in enumerate(frames):
                frame_vals = {k: get(frame, k) for k in frame_fields}
                dps = get(frame, "dataInfo")
                for d_idx, dp in enumerate(dps):
                    # pull next name (or empty if we ran out)
                    data_name = all_data_names[flat_idx] if flat_idx < len(all_data_names) else ""
                    flat_idx += 1

                    dp_vals = {}
                    for k in dp_fields:
                        java_key = dp_field_map.get(k, k)
                        dp_vals[java_key] = get(dp, k)

                    row = {
                        "nodeID": node_info.node_id,
                        "frameIndex": f_idx,
                        "dataIndex": d_idx,
                        "nodeName": node_info.name,
                        "numFrames": node_info.num_frames,
                        "dataName": data_name,
                        "plottable": "true", # CAN frame data is always considered plottable
                        **frame_vals,
                        **dp_vals,
                    }
                    w.writerow(row)

        telemetry_id = expression_to_int("specialIDs::telemetryID")
        assert(telemetry_id >= 0), "specialIDs::telemetryID not found in global defines. Cannot add telemetry-specific packet fields."
        
        for packet_idx, packet in enumerate(vitals_to_telem_packets):
            packet_name = packet['name']
            msg_fields = packet.get("msgFields", [])

            # Add numData to the packet dict so it gets written to the CSV
            # This fixes the "Mismatched data count" error for telemetry-specific packets.
            packet['numData'] = len(msg_fields)

            # All fields from vitals_to_telem packets are now added to the CSV.
            for field_idx, field in enumerate(msg_fields):
                row = {
                    "nodeID": telemetry_id,
                    "frameIndex": packet_idx,
                    "dataIndex": field_idx,
                    "nodeName": "telemetry",
                    "numFrames": len(vitals_to_telem_packets),
                    "dataName": f"{packet_name}_{field.name}",
                    "plottable": str(getattr(field, 'plottable', False)).lower(),
                }

                # Add frame-level fields from the packet definition
                for k in frame_fields:
                    val = packet.get(k, 0)
                    if k == 'dataTimeout':
                        if(val != 0): #time of 0 indicates no timeout
                            val = max(val, TELEMETRY_MIN_TIMEOUT)
                    row[k] = val

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

def createTelemetry(
    nodes: list[Node],
    fields: ParsedFields,
    vitals_to_telem_packets: list[dict[str, Any]],
    telem_to_vitals_packets: list[dict[str, Any]],
    telemetry_presentation_dir: str,
    telemetry_csv_path: str,
    telemetry_records_path: str
) -> None:
    from Lora_Msgs_And_Cmds.genTelemetryCallbacks import generate_all_telemetry_callbacks

    telemetry_parser_lut_path = os.path.join(telemetry_presentation_dir, "TelemetryParserLUT.java")
    telemetry_packet_visitor_path = os.path.join(telemetry_presentation_dir, "GeneratedPacketVisitor.java")
    telemetry_command_records_path = os.path.join(telemetry_presentation_dir, "CommandRecords.java")
    telemetry_callbacks_dir = os.path.normpath(
        os.path.join(telemetry_presentation_dir, "..", "application", "callbacks")
    )

    genTelemCSV(nodes, fields, telemetry_csv_path, vitals_to_telem_packets)
    createTelemetryParserLUT(vitals_to_telem_packets, telemetry_parser_lut_path)
    createTelemetryRecords(fields, telemetry_records_path)
    generate_all_telemetry_callbacks(
        vitals_to_telem_packets,
        telem_to_vitals_packets,
        fields.globalEnums,
        nodes,
        telemetry_packet_visitor_path,
        telemetry_presentation_dir,
        telemetry_callbacks_dir,
        telemetry_command_records_path,
    )
