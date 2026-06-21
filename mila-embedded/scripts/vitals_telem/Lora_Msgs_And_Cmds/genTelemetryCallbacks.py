import os
from itertools import groupby
from typing import Any

from config.parseFile import ACCESS, GlobalEnum, Node
from genTelemetry import createCommandRecords


def generate_java_visitor_dispatcher(
    output_path: str,
    vitals_to_telem_packets: list[dict[str, Any]],
) -> None:
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    print(f"Generating Java visitor dispatcher at {os.path.relpath(output_path)}...")
    with open(output_path, 'w') as f:
        f.write("/**\n")
        f.write(" * This is a fully auto-generated file. DO NOT EDIT.\n")
        f.write(" * It implements the PacketVisitor and delegates each packet to a handler\n")
        f.write(" * in the 'callbacks' package. Skeletons for those handlers are generated\n")
        f.write(" * once and are safe for user modification.\n")
        f.write(" */\n")
        f.write("package presentation;\n\n")
        f.write("import application.UI.NotificationPanel;\n")
        f.write("import application.DataHandler;\n")
        f.write("import application.callbacks.*;\n")
        f.write("import lookup.TelemetryLookup;\n")
        f.write("public class GeneratedPacketVisitor implements TelemetryParserLUT.PacketVisitor {\n\n")
        f.write("    private final TelemetryLookup lookup;\n")
        f.write("    private final NotificationPanel notifications;\n")
        f.write("    private final DataHandler dataHandler;\n\n")
        f.write("    public GeneratedPacketVisitor(TelemetryLookup lookup, NotificationPanel notifications, DataHandler dataHandler) {\n")
        f.write("        this.lookup = lookup;\n")
        f.write("        this.notifications = notifications;\n")
        f.write("        this.dataHandler = dataHandler;\n")
        f.write("    }\n\n")

        for msg in vitals_to_telem_packets:
            class_name = f"TelemetryParserLUT.{msg['name']}Packet"
            handler_class = f"On{msg['name']}Packet"
            is_custom = msg.get("containsPayload", False)
            extra_params = ", BitStream stream" if is_custom else ""
            handle_params = "p, stream, dataHandler, notifications, lookup" if is_custom else "p, dataHandler, notifications, lookup"

            f.write("    @Override\n")
            f.write(f"    public void visit({class_name} p{extra_params}) {{\n")
            f.write(f"        new {handler_class}().handle({handle_params});\n")
            if not is_custom:
                f.write("        return;\n")
            f.write("    }\n\n")

        f.write("    @Override\n")
        f.write("    public void visit(TelemetryParserLUT.ParsedPacket p) {\n")
        f.write("        // Fallback for unhandled packets\n")
        f.write("    }\n")
        f.write("}\n")

    with open(output_path, 'r') as f:
        lines = f.readlines()

    if any(msg.get("containsPayload", False) for msg in vitals_to_telem_packets):
        for i, line in enumerate(lines):
            if "package presentation;" in line:
                lines.insert(i + 1, "import presentation.BitStream;\n")
                break

    with open(output_path, 'w') as f:
        f.writelines(lines)


def generate_single_java_callback_skeleton(
    output_path: str,
    packet: dict[str, Any],
) -> None:
    packet_name = packet['name']
    class_name = f"On{packet_name}Packet"
    packet_class_name = f"TelemetryParserLUT.{packet_name}Packet"
    is_custom = packet.get("containsPayload", False)
    extra_params = ", BitStream stream" if is_custom else ""

    print(f"Generating initial Java callback skeleton for {packet_name} at {os.path.relpath(output_path)}...")
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, 'w') as f:
        f.write("/**\n")
        f.write(f" * This is a user-editable file for handling {packet_name} packets.\n")
        f.write(" * It is generated once and will not be overwritten.\n")
        f.write(" */\n")
        f.write("package application.callbacks;\n\n")
        f.write("import application.UI.NotificationPanel;\n")
        f.write("import application.DataHandler;\n")
        f.write("import lookup.TelemetryLookup;\n")
        f.write("import presentation.TelemetryParserLUT;\n")

        if is_custom:
            f.write("import presentation.BitStream;\n")
            f.write("import java.nio.ByteBuffer;\n")
            f.write("import java.nio.ByteOrder;\n")
        f.write("\n")

        f.write(f"public class {class_name} {{\n\n")
        f.write(f"    public void handle({packet_class_name} p{extra_params}, DataHandler dataHandler, NotificationPanel notifications, TelemetryLookup lookup) {{\n")
        f.write(f"        // TODO: Implement any additional logic for {packet_name}\n")
        if is_custom:
            f.write("        // You can now read directly from the 'stream' object.\n")
            f.write("        // The parser will continue from where the stream is left.\n")
            f.write("        // Example: Optional<byte[]> payload = stream.readBytes(p.numErrors() * 2, \"error_codes\");\n")
        f.write("    }\n\n")
        f.write("}\n")


def _generate_can_parser(
    output_path: str,
    callback_frames: list[dict[str, Any]],
    data_names: list[str],
) -> None:
    with open(output_path, "w") as f:
        f.write("/** Auto-generated file. Do not edit. */\n\n")
        f.write("package presentation;\n\n")
        f.write("import java.util.Optional;\n\n")
        f.write("public final class CANFrameParser {\n\n")
        f.write("    private CANFrameParser() {}\n\n")
        f.write("    public interface CANFrameVisitor {\n")
        for frame in callback_frames:
            packet_name = f"{frame['node_name']}_Frame{frame['frame_idx']}"
            f.write(f"        void visit({packet_name}Packet p);\n")
        f.write("    }\n\n")
        f.write("    public static abstract class ParsedCANFrame {\n")
        f.write("        public final int nodeId; public final int frameIndex;\n")
        f.write("        protected ParsedCANFrame(int n, int f) { this.nodeId = n; this.frameIndex = f; }\n")
        f.write("        public abstract void accept(CANFrameVisitor visitor);\n")
        f.write("    }\n\n")

        for frame in callback_frames:
            packet_name = f"{frame['node_name']}_Frame{frame['frame_idx']}"
            class_name = f"{packet_name}Packet"
            f.write(f"    public static class {class_name} extends ParsedCANFrame {{\n")
            f.write("        public final int[] values;\n")
            f.write(f"        public {class_name}(int[] v) {{ super({frame['node_id']}, {frame['frame_idx']}); this.values = v; }}\n")
            for i in range(frame['num_data']):
                data_name = data_names[frame['data_start_idx'] + i]
                f.write(f"        public int {data_name}() {{ return values[{i}]; }}\n")
            f.write("        @Override\n")
            f.write("        public void accept(CANFrameVisitor visitor) { visitor.visit(this); }\n")
            f.write("    }\n\n")

        f.write("    public static Optional<ParsedCANFrame> createPacket(int nodeId, int frameIndex, int[] values) {\n")
        f.write("        switch (nodeId) {\n")
        callback_frames.sort(key=lambda x: x['node_id'])
        for node_id, group_iter in groupby(callback_frames, key=lambda x: x['node_id']):
            group = list(group_iter)
            f.write(f"            case {node_id}: // {group[0]['node_name']}\n")
            f.write("                switch (frameIndex) {\n")
            for frame in group:
                packet_name = f"{frame['node_name']}_Frame{frame['frame_idx']}"
                class_name = f"{packet_name}Packet"
                f.write(f"                    case {frame['frame_idx']}: return Optional.of(new {class_name}(values));\n")
            f.write("                    default: return Optional.empty();\n")
            f.write("                }\n")
        f.write("            default: return Optional.empty();\n")
        f.write("        }\n")
        f.write("    }\n")
        f.write("}\n")


def _generate_can_visitor_dispatcher(
    output_path: str,
    callback_frames: list[dict[str, Any]],
) -> None:
    with open(output_path, 'w') as f:
        f.write("/** Auto-generated file. Do not edit. */\n\n")
        f.write("package presentation;\n\n")
        f.write("import application.UI.MainPanel;\n")
        f.write("import application.UI.NotificationPanel;\n")
        f.write("import application.callbacks.can.*;\n")
        f.write("import lookup.TelemetryLookup;\n\n")
        f.write("public class GeneratedCANFrameVisitor implements CANFrameParser.CANFrameVisitor {\n\n")
        f.write("    private final TelemetryLookup lookup;\n")
        f.write("    private final NotificationPanel notifications;\n")
        f.write("    private final MainPanel mainPanel;\n\n")
        f.write("    public GeneratedCANFrameVisitor(TelemetryLookup lookup, NotificationPanel notifications, MainPanel mainPanel) {\n")
        f.write("        this.lookup = lookup;\n")
        f.write("        this.notifications = notifications;\n")
        f.write("        this.mainPanel = mainPanel;\n")
        f.write("    }\n\n")

        for frame in callback_frames:
            packet_name = f"{frame['node_name']}_Frame{frame['frame_idx']}"
            packet_class = f"CANFrameParser.{packet_name}Packet"
            handler_class = f"On{packet_name}Packet"
            f.write("    @Override\n")
            f.write(f"    public void visit({packet_class} p) {{\n")
            f.write(f"        new {handler_class}().handle(p, mainPanel, notifications, lookup);\n")
            f.write("    }\n\n")
        f.write("}\n")


def _generate_can_callback_skeleton(
    output_path: str,
    packet_name: str,
    frame_info: dict[str, Any],
    data_names: list[str],
) -> None:
    class_name = f"On{packet_name}Packet"
    packet_class_name = f"CANFrameParser.{packet_name}Packet"
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, 'w') as f:
        f.write("/**\n * This is a user-editable file for handling CAN frames.\n")
        f.write(" * It is generated once and will not be overwritten.\n")
        f.write(" */\n")
        f.write("package application.callbacks.can;\n\n")
        f.write("import application.UI.MainPanel;\n")
        f.write("import application.UI.NotificationPanel;\n")
        f.write("import lookup.TelemetryLookup;\n")
        f.write("import presentation.CANFrameParser;\n\n")
        f.write(f"public class {class_name} {{\n\n")
        f.write(f"    public void handle({packet_class_name} p, MainPanel mainPanel, NotificationPanel notifications, TelemetryLookup lookup) {{\n")
        f.write("        // This callback is fired when a CAN frame with 'enableTelemCallback=true' is received.\n")
        f.write("        // The data has already been parsed, plotted, and checked for timeouts.\n")
        first_data_name = data_names[frame_info['data_start_idx']] if frame_info['num_data'] > 0 else "yourData"
        f.write(f"        // You can access the data via p.dataName() methods, e.g., p.{first_data_name}()\n")
        f.write("        \n")
        f.write("    }\n\n")
        f.write("}\n")


def create_can_frame_callbacks(
    nodes: list[Any],
    callback_dir: str,
    visitor_dir: str,
) -> None:
    callback_frames = []
    global_data_idx = 0
    all_data_names = [name for node in nodes for name in node.data_names]

    for node_info in nodes:
        node_vitals_data = node_info.vitals_data
        frames = ACCESS(node_vitals_data, "CANFrames")["value"]
        for frame_idx_in_node, frame in enumerate(frames):
            num_data_in_frame = len(ACCESS(frame, "dataInfo")["value"])
            if ACCESS(frame, "enableTelemCallback")["value"]:
                callback_frames.append({
                    "node_id": node_info.node_id,
                    "node_name": node_info.name,
                    "frame_idx": frame_idx_in_node,
                    "num_data": num_data_in_frame,
                    "data_start_idx": global_data_idx,
                })
            global_data_idx += num_data_in_frame

    if not callback_frames:
        return

    _generate_can_parser(os.path.join(visitor_dir, 'CANFrameParser.java'), callback_frames, all_data_names)
    _generate_can_visitor_dispatcher(os.path.join(visitor_dir, 'GeneratedCANFrameVisitor.java'), callback_frames)

    for frame_info in callback_frames:
        packet_name = f"{frame_info['node_name']}_Frame{frame_info['frame_idx']}"
        skeleton_path = os.path.join(callback_dir, 'can', f"On{packet_name}Packet.java")
        if not os.path.exists(skeleton_path):
            print(f"Generating initial CAN callback skeleton for {packet_name}...")
            _generate_can_callback_skeleton(skeleton_path, packet_name, frame_info, all_data_names)


def generate_all_telemetry_callbacks(
    vitals_to_telem: list[dict[str, Any]],
    telem_to_vitals: list[dict[str, Any]],
    globalEnums: list[GlobalEnum],
    nodes: list[Node],
    generated_packet_visitor_path: str,
    presentation_dir: str,
    java_callbacks_dir: str,
    command_records_path: str,
) -> None:
    print("\n--- Generating Vitals->Telem Callback Skeletons (Java) ---")
    generate_java_visitor_dispatcher(generated_packet_visitor_path, vitals_to_telem)

    os.makedirs(java_callbacks_dir, exist_ok=True)
    for packet in vitals_to_telem:
        skeleton_path = os.path.join(java_callbacks_dir, f"On{packet['name']}Packet.java")
        if not os.path.exists(skeleton_path):
            generate_single_java_callback_skeleton(skeleton_path, packet)

    print("\n--- Generating CAN Frame Callback Skeletons (Java) ---")
    create_can_frame_callbacks(nodes, java_callbacks_dir, presentation_dir)

    print("\n--- Generating Command Record Files (Java) ---")
    createCommandRecords(telem_to_vitals, globalEnums, command_records_path)
