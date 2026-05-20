import os
from parseFile import ACCESS
from itertools import groupby

def get_telem_path():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    java_dir = os.path.join(script_dir, "..", "..", "telem-dashboard", "src", "main")
    return os.path.normpath(java_dir)

def _generate_can_parser(output_path, callback_frames, data_names):
    with open(output_path, "w") as f:
        f.write("/** Auto-generated file. Do not edit. */\n\n")
        f.write("public final class CANFrameParser {\n\n")
        f.write("    private CANFrameParser() {}\n\n")

        # Visitor Interface
        f.write("    public interface CANFrameVisitor {\n")
        for frame in callback_frames:
            packet_name = f"{frame['node_name']}_Frame{frame['frame_idx']}"
            f.write(f"        void visit({packet_name}Packet p);\n")
        f.write("    }\n\n")

        # Base Packet Class
        f.write("    public static abstract class ParsedCANFrame {\n")
        f.write("        public final int nodeId; public final int frameIndex;\n")
        f.write("        protected ParsedCANFrame(int n, int f) { this.nodeId = n; this.frameIndex = f; }\n")
        f.write("        public abstract void accept(CANFrameVisitor visitor);\n")
        f.write("    }\n\n")

        # Concrete Packet Classes
        for frame in callback_frames:
            packet_name = f"{frame['node_name']}_Frame{frame['frame_idx']}"
            class_name = f"{packet_name}Packet"
            f.write(f"    public static class {class_name} extends ParsedCANFrame {{\n")
            f.write(f"        public final int[] values;\n")
            f.write(f"        public {class_name}(int[] v) {{ super({frame['node_id']}, {frame['frame_idx']}); this.values = v; }}\n")
            for i in range(frame['num_data']):
                data_name = data_names[frame['data_start_idx'] + i]
                f.write(f"        public int {data_name}() {{ return values[{i}]; }}\n")
            f.write("        @Override\n")
            f.write(f"        public void accept(CANFrameVisitor visitor) {{ visitor.visit(this); }}\n")
            f.write("    }\n\n")
        
        # createPacket Factory Method
        f.write("    public static ParsedCANFrame createPacket(int nodeId, int frameIndex, int[] values) {\n")
        f.write("        switch (nodeId) {\n")
        
        callback_frames.sort(key=lambda x: x['node_id'])
        for node_id, group_iter in groupby(callback_frames, key=lambda x: x['node_id']):
            group = list(group_iter)
            f.write(f"            case {node_id}: // {group[0]['node_name']}\n")
            f.write("                switch (frameIndex) {\n")
            for frame in group:
                packet_name = f"{frame['node_name']}_Frame{frame['frame_idx']}"
                class_name = f"{packet_name}Packet"
                f.write(f"                    case {frame['frame_idx']}: return new {class_name}(values);\n")
            f.write("                    default: return null;\n")
            f.write("                }\n")

        f.write("            default: return null;\n")
        f.write("        }\n")
        f.write("    }\n")
        f.write("}\n")

def _generate_can_visitor_dispatcher(output_path, callback_frames):
    with open(output_path, 'w') as f:
        f.write("/** Auto-generated file. Do not edit. */\n\n")
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
            f.write(f"    @Override\n")
            f.write(f"    public void visit({packet_class} p) {{\n")
            f.write(f"        new {handler_class}().handle(p, mainPanel, notifications, lookup);\n")
            f.write("    }\n\n")
        f.write("}\n")

def _generate_can_callback_skeleton(output_path, packet_name, frame_info, data_names):
    class_name = f"On{packet_name}Packet"
    packet_class_name = f"CANFrameParser.{packet_name}Packet"
    
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, 'w') as f:
        f.write("/**\n * This is a user-editable file for handling CAN frames.\n")
        f.write(" * It is generated once and will not be overwritten.\n")
        f.write(" */\n")
        f.write(f"public class {class_name} {{\n\n")
        f.write(f"    public void handle({packet_class_name} p, MainPanel mainPanel, NotificationPanel notifications, TelemetryLookup lookup) {{\n")
        f.write(f"        // This callback is fired when a CAN frame with 'enableTelemCallback=true' is received.\n")
        f.write(f"        // The data has already been parsed, plotted, and checked for timeouts.\n")
        first_data_name = data_names[frame_info['data_start_idx']] if frame_info['num_data'] > 0 else "yourData"
        f.write(f"        // You can access the data via p.dataName() methods, e.g., p.{first_data_name}()\n")
        f.write("    }\n\n")
        f.write("}\n")

def create_can_frame_callbacks(vitals_nodes, node_names, node_ids, data_names):
    java_dir = get_telem_path()
    
    callback_frames = []
    global_data_idx = 0
    for node_idx, node in enumerate(vitals_nodes):
        frames = ACCESS(node, "CANFrames")["value"]
        for frame_idx_in_node, frame in enumerate(frames):
            num_data_in_frame = len(ACCESS(frame, "dataInfo")["value"])
            if ACCESS(frame, "enableTelemCallback")["value"]:
                callback_frames.append({
                    "node_id": node_ids[node_idx],
                    "node_name": node_names[node_idx],
                    "frame_idx": frame_idx_in_node,
                    "num_data": num_data_in_frame,
                    "data_start_idx": global_data_idx,
                })
            global_data_idx += num_data_in_frame

    if not callback_frames: return

    _generate_can_parser(os.path.join(java_dir, 'java', 'CANFrameParser.java'), callback_frames, data_names)
    _generate_can_visitor_dispatcher(os.path.join(java_dir, 'java', 'GeneratedCANFrameVisitor.java'), callback_frames)
    
    for frame_info in callback_frames:
        packet_name = f"{frame_info['node_name']}_Frame{frame_info['frame_idx']}"
        skeleton_path = os.path.join(java_dir, 'java', 'callbacks', 'can', f"On{packet_name}Packet.java")
        if not os.path.exists(skeleton_path):
            print(f"Generating initial CAN callback skeleton for {packet_name}...")
            _generate_can_callback_skeleton(skeleton_path, packet_name, frame_info, data_names)