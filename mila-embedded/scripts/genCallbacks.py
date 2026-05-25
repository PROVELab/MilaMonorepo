import os
from packetFormat import CUSTOM

def generate_java_visitor_dispatcher(output_dir, vitals_to_telem_packets):
    os.makedirs(os.path.dirname(output_dir), exist_ok=True)
    output_path = os.path.join(output_dir, 'GeneratedPacketVisitor.java')
    """
    Generates a fully-generated Java `PacketVisitor implementation that
    delegates to user-editable handlers in the 'callbacks' package.
    """
    print(f"Generating Java visitor dispatcher at {os.path.relpath(output_path)}...")
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, 'w') as f:
        f.write("/**\n")
        f.write(" * This is a fully auto-generated file. DO NOT EDIT.\n")
        f.write(" * It implements the PacketVisitor and delegates each packet to a handler\n")
        f.write(" * in the 'callbacks' package. Skeletons for those handlers are generated\n")
        f.write(" * once and are safe for user modification.\n")
        f.write(" */\n\n")
        f.write("public class GeneratedPacketVisitor implements TelemetryParserLUT.PacketVisitor {\n\n")
        f.write("    private final TelemetryLookup lookup;\n")
        f.write("    private final NotificationPanel notifications;\n")
        f.write("    private final MainPanel mainPanel;\n\n")
        f.write("    public GeneratedPacketVisitor(TelemetryLookup lookup, NotificationPanel notifications, MainPanel mainPanel) {\n")
        f.write("        this.lookup = lookup;\n")
        f.write("        this.notifications = notifications;\n")
        f.write("        this.mainPanel = mainPanel;\n")
        f.write("    }\n\n")

        for msg in vitals_to_telem_packets:
            class_name = f"TelemetryParserLUT.{msg['name']}Packet"
            handler_class = f"On{msg['name']}Packet"
            is_custom = msg.get("byteCount") is CUSTOM
            return_type = "int" if is_custom else "void"
            extra_params = ", BitStream stream" if is_custom else ""

            f.write(f"    @Override\n")
            f.write(f"    public {return_type} visit({class_name} p{extra_params}) {{\n")
            if is_custom:
                f.write(f"        return new {handler_class}().handle(p, stream, mainPanel, notifications, lookup);\n")
            else:
                f.write(f"        new {handler_class}().handle(p, mainPanel, notifications, lookup);\n")
            f.write("    }\n\n")

        f.write("    @Override\n")
        f.write("    public void visit(TelemetryParserLUT.ParsedPacket p) {\n")
        f.write("        // Fallback for unhandled packets\n")
        f.write("    }\n")
        f.write("}\n")

def generate_single_java_callback_skeleton(output_path, packet):
    """
    Generates a skeleton handler for a single packet type.
    """
    packet_name = packet['name']
    class_name = f"On{packet_name}Packet"
    packet_class_name = f"TelemetryParserLUT.{packet_name}Packet"
    is_custom = packet.get("byteCount") is CUSTOM
    return_type = "int" if is_custom else "void"
    extra_params = ", BitStream stream" if is_custom else ""
    return_statement = "        return 0; // Return number of bytes consumed from custom payload\n" if is_custom else ""

    print(f"Generating initial Java callback skeleton for {packet_name} at {os.path.relpath(output_path)}...")
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, 'w') as f:
        f.write("/**\n")
        f.write(f" * This is a user-editable file for handling {packet_name} packets.\n")
        f.write(" * It is generated once and will not be overwritten.\n")
        f.write(" */\n")
        if is_custom:
            f.write("import java.nio.ByteBuffer;\nimport java.nio.ByteOrder;\n\n")
        f.write(f"public class {class_name} {{\n\n")
        f.write(f"    public {return_type} handle({packet_class_name} p{extra_params}, MainPanel mainPanel, NotificationPanel notifications, TelemetryLookup lookup) {{\n")
        f.write(f"        // TODO: Implement any additional logic for {packet_name}\n")
        f.write(return_statement)
        f.write("    }\n\n")
        f.write("}\n")

def _generate_cpp_callbacks_content(f, telem_to_vitals):
    """
    Writes the content for a skeleton vitalsRecvCallbacks.cpp file.
    """
    f.write('#include "../vitalsHelper/vitalsPacketRecvLUT.h"\n')
    f.write('// #include "vitals/vitals.h" // No longer needed here, forwarding handled in LUT\n')
    f.write('#include "esp_log.h"\n\n')
    f.write('static const char* TAG = "VitalsRecvCallbacks";\n\n')
    f.write("/**\n * @brief These are auto-generated stub implementations for the packet receiver callbacks.\n"
            " * This file is generated once and will not be overwritten.\n"
            " *\n * The code generator declares the function prototypes in vitalsPacketRecvLUT.h\n"
            " * and calls them from the parser in vitalsPacketRecvLUT.c.\n"
            " *\n * You can add your application-specific logic to these functions.\n"
            " * For packets not targeted at this \"vitals\" node, a forwarding implementation is provided.\n"
            " */\n\n")

    for msg in telem_to_vitals:
        name = msg["name"]
        fields = msg.get("msgFields", [])
        byte_count = msg.get("byteCount")
        struct_name = f"{name}_args_t"
        target_node = msg.get("targetNode", "vitals")

        # Generate function signature
        params = []
        has_struct = len(fields) > 0 or byte_count is CUSTOM
        if has_struct:
            params.append(f"{struct_name} args")

        param_str = ", ".join(params) if params else "void"
        return_type = "size_t" if byte_count is CUSTOM else "void"
        f.write(f"{return_type} on{name}({param_str}) {{\n")

        # Generate function body
        log_args = []
        if fields:
            for field in fields:
                if(field.enum is not None and field.enum):
                    log_args.append(f"args.{field.name}.i32")   #for unions, access the i32 interpretation of the value
                else:
                    log_args.append(f"args.{field.name}")
        
        log_prefix = f"Callback on{name} called."
        if target_node != "vitals":
            log_prefix += f" (packet was forwarded to {target_node} by Vitals)."

        if fields:
            # Safely build the C++ string literal, macro by macro
            fmt_string = f'"{log_prefix} '
            
            for i, field in enumerate(fields):
                fmt_string += f'{field.name}: %" PRId32'
                    
                
                # If there are more fields, reopen the quote and add a comma separator
                if i < len(fields) - 1:
                    fmt_string += ' ", '
            
            # Combine the arguments
            args_string = ', '.join(log_args)
            
            # Write the complete, perfectly balanced line
            f.write(f'    ESP_LOGI(TAG, {fmt_string}, {args_string});\n')
        else:
            # Clean fallback for no fields
            f.write(f'    ESP_LOGI(TAG, "{log_prefix}");\n')

        f.write(f"    // TODO: Implement logic for {name}\n")

        if byte_count is CUSTOM:
             f.write("    // Access payload via args.payload, with max size args.max_payload_size\n")
             f.write("    // Example: ESP_LOG_BUFFER_HEX(TAG, args.payload, args.max_payload_size);\n")
             f.write("    return 0; // Return number of payload bytes consumed\n")

        f.write("}\n\n")

def generate_cpp_callbacks(output_path, telem_to_vitals):
    """
    Generates the vitalsRecvCallbacks.cpp file.
    """
    print(f"Generating initial C++ callbacks at {os.path.relpath(output_path)}...")
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, 'w') as f:
        _generate_cpp_callbacks_content(f, telem_to_vitals)