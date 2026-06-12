import os
from Lora_Msgs_And_Cmds.packetFormat import FIXED, CUSTOM
from config.parseFile import ACCESS, globalEnums, globalDefines
import math

def createPacketSendFiles(generated_code_dir, vitals_helper_dir, vitals_node_dir, nodes, vitals_to_telem, telem_to_vitals, globalEnums):

    """
    Generates C source and header files for sending formatted telemetry packets.
    - vitalsPacketSendLUT.h/c: Contains lookup tables for packet message fields.
    """

    header_path = os.path.join(vitals_helper_dir, "vitalsPacketSendLUT.h")
    source_path = os.path.join(vitals_helper_dir, "vitalsPacketSendLUT.c")
    recv_header_path = os.path.join(vitals_helper_dir, "vitalsPacketRecvLUT.h")
    recv_source_path = os.path.join(vitals_helper_dir, "vitalsPacketRecvLUT.c")
    # recv_callbacks_path is now handled by genCallbacks.py, called from codeGen_main.py
    mapping_path = os.path.join(generated_code_dir, "mask_mappings.txt")

    node_id_map = {node.name: node.id for node in nodes}

    # --- Assign CAN-level masks for forwarded packets ---
    forwarded_packets_by_node = {}

    # Sanity check: CUSTOM packets must be byte-aligned after their mask and fixed fields.
    for msg in telem_to_vitals:
        if msg.get("byteCount") is CUSTOM:
            # This check happens after PACK_MINIMUM_BITS is resolved to an integer by the huffman-like assignment.
            mask_bits = msg.get("mask_bits", 0)
            
            fixed_field_bits = 0
            for f in msg.get("msgFields", []):
                # A field's bits can be a callable, but for CUSTOM packets they are expected to be fixed integers.
                fixed_field_bits += f.bits
            
            total_header_bits = mask_bits + fixed_field_bits
            assert total_header_bits % 8 == 0, \
                f"CUSTOM packet '{msg['name']}' is not byte-aligned. Its header (mask_bits={mask_bits} + fixed_fields={fixed_field_bits}) is {total_header_bits}, which is not a multiple of 8. Please adjust field or mask bit lengths in packetFormat.py."

    # 2. Generate the Header Code
    with open(header_path, 'w') as f:
        f.write("#ifndef VITALS_PACKET_SEND_LUT_H\n")
        f.write("#define VITALS_PACKET_SEND_LUT_H\n\n")
        f.write("#ifdef __cplusplus\n")
        f.write("extern \"C\" {\n")
        f.write("#endif\n\n")
        f.write('#include "pecan/pecan.h"\n')
        f.write('#include <stddef.h>\n')
        f.write('#include <stdint.h>\n')
        f.write('#include "freertos/FreeRTOS.h"\n')
        f.write('#include "freertos/semphr.h"\n')
        f.write('#include <string.h> // For memcpy\n\n')
        
        # Add rate controller struct and extern declaration
        f.write("// Rate limiting for vitals-to-telemetry packets\n")
        f.write("typedef struct {\n")
        f.write("    uint8_t divider; // Send every Nth call. 1 = send every time.\n")
        f.write("    uint8_t counter; // Internal counter.\n")
        f.write("} VitalsSendRateController;\n\n")
        f.write("#include \"../../programConstants.h\" // For numVitalsToTelemPackets\n")
        f.write("extern VitalsSendRateController vitals_send_rate_controllers[numVitalsToTelemPackets];\n\n")

        f.write("#include \"../vitalsSendData.h\"\n")

        for msg in vitals_to_telem:
            name = msg["name"]
            byte_count = msg.get("byteCount")
            mask_bits = msg.get("mask_bits", 0)
            mask_val = msg.get("mask", 0)
            packet_idx = msg.get("packet_idx")
            
            fields = msg.get("msgFields", [])

            for field in fields:
                assert(field.max <= 2147483647)
                assert(field.min >= -2147483648)
            
            # Array size calculations
            has_struct = len(fields) > 0
            total_fields = len(fields) + (1 if mask_bits > 0 else 0)
            totalBits = mask_bits + sum(f.bits for f in fields)
            num_bytes = (totalBits + 7) // 8 
            
            f.write(f"// ----- {name} -----\n")

            struct_name = f"send{name}Args" # Full struct for FIXED packets
            header_struct_name = f"send{name}Header" # Header-only struct for CUSTOM packets

            if has_struct:
                # For CUSTOM, define a header-only struct. For FIXED, define the full struct.
                struct_to_unionize = header_struct_name if byte_count is CUSTOM else struct_name

                # Pre-define field-specific unions for fields that are enums
                enum_fields = {} # Store enum type for fields that are enums
                for field in fields:
                    if field.enum:
                        enum_type_name = field.enum if isinstance(field.enum, str) else field.name
                        enum_fields[field.name] = enum_type_name
                        union_type_name = f"union_{name}_{field.name}"
                        f.write(f"typedef union {{\n")
                        f.write(f"    int32_t i32;\n")
                        f.write(f"    {enum_type_name} e;\n")
                        f.write(f"}} {union_type_name};\n\n")

                f.write(f"typedef struct __attribute__((packed)) {struct_to_unionize}{{\n")
                if mask_bits > 0:
                    f.write(f"    int32_t mask;\n")
                
                # Use the unions in the struct definition where applicable
                for field in fields:
                    if field.name in enum_fields:
                        union_type_name = f"union_{name}_{field.name}"
                        f.write(f"    {union_type_name} {field.name};\n")
                    else:
                        f.write(f"    int32_t {field.name};\n")
                f.write(f"}} {struct_to_unionize};\n\n")

                # Define a union for the int32_t fields for safe type-punning
                union_name = f"union_send{name}"
                f.write(f"typedef union {{\n")
                f.write(f"    {struct_to_unionize} s;\n")
                f.write(f"    int32_t data_arr[{total_fields}];\n")
                f.write(f"}} {union_name};\n\n")

            field_ptr = f"{name}_fields" if total_fields > 0 else "NULL"
            if total_fields > 0:
                f.write(f"extern const simpleDataPoint {name}_fields[{total_fields}];\n")

            params_with_types = []
            params_no_types = []
            if byte_count is CUSTOM:
                if has_struct:
                    params_with_types.append(f"{header_struct_name} header")
                    params_no_types.append("header")
                params_with_types.append("const uint8_t* payload")
                params_with_types.append("size_t payloadBytes")
                params_no_types.extend(["payload", "payloadBytes"])
            elif has_struct:
                params_with_types.append(f"{struct_name} args")
                params_no_types.append("args")

            params_with_types_format = params_with_types.copy()
            params_with_types_format.append("uint8_t* out_buffer")

            params_with_types_format = params_with_types.copy()
            params_with_types_format.append("uint8_t* out_buffer")

            # --- format<name>Function ---
            f.write(f"static inline uint8_t format{name}Function({', '.join(params_with_types_format)}) {{\n")
            if byte_count is CUSTOM:
                if has_struct:
                    if mask_bits > 0:
                        f.write(f"    header.mask = (int32_t){mask_val}; // Auto-assigned\n")
                    f.write(f"    {union_name} u __attribute__((aligned(4)));\n")
                    f.write(f"    u.s = header;\n")
                    f.write(f"    return formatPacketVariable({field_ptr}, {total_fields}, u.data_arr, payload, payloadBytes, out_buffer);\n")
                else:
                    if mask_bits > 0:
                        f.write(f"    int32_t data[1] = {{(int32_t){mask_val}}};\n")
                        f.write(f"    return formatPacketVariable({field_ptr}, {total_fields}, data, payload, payloadBytes, out_buffer);\n")
                    else:
                        f.write(f"    return formatPacketVariable({field_ptr}, {total_fields}, NULL, payload, payloadBytes, out_buffer);\n")
            elif byte_count is FIXED:
                if has_struct:
                    if mask_bits > 0:
                        f.write(f"    args.mask = (int32_t){mask_val}; // Auto-assigned\n")
                    f.write(f"    {union_name} u __attribute__((aligned(4)));\n")
                    f.write(f"    u.s = args;\n")
                    f.write(f"    return formatPacketCore({field_ptr}, {total_fields}, u.data_arr, out_buffer);\n")
                else:
                    if mask_bits > 0:
                        f.write(f"    int32_t data[1] = {{(int32_t){mask_val}}};\n")
                        f.write(f"    return formatPacketCore({field_ptr}, {total_fields}, data, out_buffer);\n")
                    else:
                        f.write(f"    return formatPacketCore({field_ptr}, {total_fields}, NULL, out_buffer);\n")
            f.write("}\n\n")

            # --- send<name>Function ---
            f.write(f"static inline void send{name}Function({', '.join(params_with_types)}) {{\n")
            f.write(f"    if (++vitals_send_rate_controllers[{packet_idx}].counter < vitals_send_rate_controllers[{packet_idx}].divider) return;\n")
            f.write(f"    vitals_send_rate_controllers[{packet_idx}].counter = 0;\n")
            
            if byte_count is CUSTOM:
                # CUSTOM packets use the backend's static variable buffer, so no local allocation is needed
                if has_struct:
                    if mask_bits > 0:
                        f.write(f"    header.mask = (int32_t){mask_val}; // Auto-assigned\n")
                    f.write(f"    {union_name} u __attribute__((aligned(4)));\n")
                    f.write(f"    u.s = header;\n")
                    f.write(f"    sendPacketVariable({field_ptr}, {total_fields}, u.data_arr, payload, payloadBytes);\n")
                else:
                    if mask_bits > 0:
                        f.write(f"    int32_t data[1] = {{(int32_t){mask_val}}};\n")
                        f.write(f"    sendPacketVariable({field_ptr}, {total_fields}, data, payload, payloadBytes);\n")
                    else:
                        f.write(f"    sendPacketVariable({field_ptr}, {total_fields}, NULL, payload, payloadBytes);\n")
            elif byte_count is FIXED:
                # FIXED packets still need a local buffer to pass into sendPacketCore
                f.write(f"    uint8_t dataBuffer[{num_bytes}] = {{0}};\n")
                if has_struct:
                    if mask_bits > 0:
                        f.write(f"    args.mask = (int32_t){mask_val}; // Auto-assigned\n")
                    f.write(f"    {union_name} u __attribute__((aligned(4)));\n")
                    f.write(f"    u.s = args;\n")
                    f.write(f"    sendPacketCore({field_ptr}, {total_fields}, u.data_arr, dataBuffer);\n")
                else:
                    if mask_bits > 0:
                        f.write(f"    int32_t data[1] = {{(int32_t){mask_val}}};\n")
                        f.write(f"    sendPacketCore({field_ptr}, {total_fields}, data, dataBuffer);\n")
                    else:
                        f.write(f"    sendPacketCore({field_ptr}, {total_fields}, NULL, dataBuffer);\n")
            f.write("}\n\n")
  
        f.write("\n#ifdef __cplusplus\n")
        f.write("}\n")
        f.write("#endif\n\n#endif // VITALS_PACKET_SEND_LUT_H\n")

    # --- Generate Source File ---
    with open(source_path, 'w') as f:
        f.write('#include "vitalsPacketSendLUT.h"\n\n')
        f.write("// Initialize rate controllers. Default divider is 1 (send every time).\n")
        f.write("VitalsSendRateController vitals_send_rate_controllers[numVitalsToTelemPackets] = {\n")
        for _ in vitals_to_telem:
            f.write("    {.divider = 1, .counter = 0},\n")
        f.write("};\n\n")

        # Modification 2: vitals_to_telem is a list, not a dict. Iterating directly.
        for msg in vitals_to_telem:
            byte_count = msg.get("byteCount")
            
            name = msg["name"]
            fields = msg.get("msgFields", [])
            mask_bits = msg.get("mask_bits", 0)

            total_fields = len(fields) + (1 if mask_bits > 0 else 0)

            if total_fields > 0:
                f.write(f"// ----- {name} -----\n")
                f.write(f"const simpleDataPoint {name}_fields[{total_fields}] = {{\n")
                
                if mask_bits > 0:
                    # Insert mask as the first element in the LUT
                    if mask_bits > 0:
                        mask_max = (1 << mask_bits) - 1
                    else:
                        mask_max = 0
                    f.write(f"    {{ .bits={mask_bits}, .min=0, .max={mask_max} }}, // Mask\n")

                # Insert the rest of the fields
                for field in fields:
                    f.write(f"    {{ .bits={field.bits}, .min={field.min}, .max={field.max} }},\n")
                f.write("};\n\n")

    # --- Generate Recv LUTs ---
    with open(recv_header_path, 'w') as f:
        f.write("#ifndef VITALS_PACKET_RECV_LUT_H\n")
        f.write("#define VITALS_PACKET_RECV_LUT_H\n\n")
        f.write("#ifdef __cplusplus\n")
        f.write("extern \"C\" {\n")
        f.write("#endif\n\n")
        f.write('#include "pecan/pecan.h"\n')
        f.write('#include <stddef.h>\n')
        f.write('#include <stdint.h>\n\n')

        max_recv_data_fields = 0
        if telem_to_vitals:
            max_recv_data_fields = max(len(msg.get("msgFields", [])) for msg in telem_to_vitals)
        f.write(f"#define MAX_RECV_DATA_FIELDS {max_recv_data_fields}\n") # Re-calculate max_recv_data_fields here
        f.write("#define RECV_PACKET_TYPE_FIXED 0\n")
        f.write("#define RECV_PACKET_TYPE_CUSTOM 1\n\n")

        # Generate argument structs and callback prototypes
        for msg in telem_to_vitals:
            name = msg["name"]
            fields = msg.get("msgFields", [])
            byte_count = msg.get("byteCount")
            struct_name = f"{name}_args_t"

            f.write(f"// ----- {name} -----\n")
            has_struct = len(fields) > 0 or byte_count is CUSTOM
            if has_struct:
                # Pre-define field-specific unions for fields that are enums
                enum_fields = {}
                for field in fields:
                    if field.enum:
                        enum_type_name = field.enum if isinstance(field.enum, str) else field.name
                        enum_fields[field.name] = enum_type_name
                        union_type_name = f"union_{name}_{field.name}"
                        f.write(f"typedef union {{\n")
                        f.write(f"    int32_t i32;\n")
                        f.write(f"    {enum_type_name} e;\n")
                        f.write(f"}} {union_type_name};\n\n")

                f.write(f"typedef struct __attribute__((packed)) {{\n")
                if len(fields) > 0:
                    for field in fields:
                        if field.name in enum_fields:
                            f.write(f"    union_{name}_{field.name} {field.name};\n")
                        else:
                            f.write(f"    int32_t {field.name};\n")
                if byte_count is CUSTOM:
                    f.write("    const uint8_t* payload;\n")
                    f.write("    size_t max_payload_size;\n")
                f.write(f"}} {struct_name};\n\n")

            # Generate prototype
            params = []
            if has_struct:
                params.append(f"{struct_name} args")

            param_str = ", ".join(params) if params else "void"
            return_type = "size_t" if byte_count is CUSTOM else "void"
            f.write(f"{return_type} on{name}({param_str});\n\n")

        f.write("typedef struct {\n")
        f.write("    const simpleDataPoint* fields;\n")
        f.write("    uint8_t num_fields;\n")
        f.write("    uint8_t packet_type; // RECV_PACKET_TYPE_FIXED or RECV_PACKET_TYPE_CUSTOM\n")
        f.write("    uint32_t mask_val;\n") # Store mask value
        f.write("    uint8_t mask_bits;\n") # Store mask bits
        f.write("    size_t (*callback_wrapper)(const uint8_t* raw_packet, size_t packet_len, int8_t* initial_bitIndex);\n")
        f.write("} RecvPacketLUTEntry;\n\n")
        f.write(f"extern const uint8_t MAX_RECV_MASK_BITS;\n") # Max mask bits for iteration
        f.write(f"extern const RecvPacketLUTEntry recvPacketLUT[];\n")
        f.write(f"extern const size_t recvPacketLUTSize;\n")
        f.write("\n#ifdef __cplusplus\n")
        f.write("}\n")
        f.write("#endif\n\n#endif // VITALS_PACKET_RECV_LUT_H\n")

    with open(recv_source_path, 'w') as f:
        f.write('#include "vitalsPacketRecvLUT.h"\n')
        f.write('#include "../vitalsLoraRecv.hpp"\n')
        f.write('#include "pecan/pecan.h" // For sendPacket, combinedID, etc.\n')
        f.write('#include "../../programConstants.h"\n')
        f.write('#include "esp_log.h"\n')
        f.write('#include <string.h>\n\n')
        # Generate LUT definitions
        for msg in telem_to_vitals:
            name = msg["name"]
            fields = msg.get("msgFields", [])
            mask_bits = msg.get("mask_bits", 0)

            total_fields = len(fields)
            if total_fields > 0:
                f.write(f"// ----- {name} -----\n")
                f.write(f"const simpleDataPoint {name}_fields[{total_fields}] = {{\n")
                for field in fields:
                    f.write(f"    {{ .bits={field.bits}, .min={field.min}, .max={field.max} }},\n")
                f.write("};\n\n")

        # Generate callback wrappers
        for msg in telem_to_vitals:
            name = msg["name"]
            # Handle potential typo in packetFormat.py for backwards compatibility
            target_node = msg.get("targetNode", msg.get("targetNode:", "vitals"))

            fields = msg.get("msgFields", [])
            byte_count = msg.get("byteCount")
            struct_name = f"{name}_args_t"
            wrapper_name = f"on{name}_wrapper"

            f.write(f"// Wrapper for {name}\n")
            f.write(f"static size_t {wrapper_name}(const uint8_t* raw_packet, size_t packet_len, int8_t* bitIndex) {{\n")
            # Forwarding logic
            if target_node != "vitals":
                node_id = node_id_map.get(target_node)
                if node_id is None:
                    # It must be an enum value like 'prechargeID'
                    node_id_str = target_node
                if node_id is None:
                    node_id_str = str(node_id_map.get(target_node, "0"))

                can_mask = msg.get('can_mask', 0)
                can_mask_bits = msg.get('can_mask_bits', 0)
                
                f.write(f"\n    // This packet is forwarded to the target node '{target_node}'.\n")
                
                field_ptr = f"{name}_fields" if len(fields) > 0 else "NULL"
                num_fields = len(fields)
                packet_type_str = f"RECV_PACKET_TYPE_{'CUSTOM' if byte_count is CUSTOM else 'FIXED'}"
                
                # Unpack just enough to forward
                if fields:
                    f.write(f"    int32_t data_arr[{len(fields)}];\n")
                    f.write(f"    for (int i = 0; i < {len(fields)}; ++i) {{\n")
                    f.write(f"        pecan_unpack(&data_arr[i], raw_packet, &{name}_fields[i], bitIndex);\n")
                    f.write(f"    }}\n")
                    args_ptr_str = "data_arr"
                else:
                    args_ptr_str = "NULL"

                f.write(f"    forwardCANPacket({node_id_str}, {can_mask}, {can_mask_bits}, {field_ptr}, {num_fields}, {packet_type_str}, {args_ptr_str}, raw_packet, packet_len, bitIndex);\n")
                f.write("    return 0; // Forwarded packets are not processed locally by vitals\n")

            else: # It's a local vitals command
                has_struct = len(fields) > 0 or byte_count is CUSTOM
                if has_struct:
                    f.write(f"    union {{ {struct_name} s; {'int32_t data_arr[' + str(len(fields)) + '];' if fields else ''} }} u __attribute__((aligned(4)));\n\n")
                    if fields:
                        f.write(f"    for (int i = 0; i < {len(fields)}; ++i) {{\n")
                        f.write(f"        pecan_unpack(&u.data_arr[i], raw_packet, &{name}_fields[i], bitIndex);\n")
                        f.write(f"    }}\n")
                
                if byte_count is CUSTOM:
                    f.write(f"    size_t fixed_bytes = (*bitIndex + 7) / 8;\n")
                    f.write(f"    if (packet_len > fixed_bytes) {{ u.s.payload = raw_packet + fixed_bytes; u.s.max_payload_size = packet_len - fixed_bytes; }} else {{ u.s.payload = NULL; u.s.max_payload_size = 0; }}\n")
                    f.write(f"    size_t custom_bytes_consumed = on{name}(u.s);\n")
                    f.write(f"    *bitIndex += custom_bytes_consumed * 8;\n")
                    f.write(f"    return 0; // For CUSTOM packets, consumption is now reflected in bitIndex.\n")
                elif has_struct:
                    f.write(f"    on{name}(u.s);\n")
                    f.write(f"    return 0;\n")
                else:
                    f.write(f"    on{name}();\n")
                    f.write(f"    return 0;\n")

            f.write("}\n\n")

        # Generate master LUT
        # Determine MAX_RECV_MASK_BITS for iteration in the receiver (needed for the C code)
        max_recv_mask_bits = 0
        if telem_to_vitals:
            max_recv_mask_bits = max(msg.get("mask_bits", 0) for msg in telem_to_vitals)
        f.write(f"const uint8_t MAX_RECV_MASK_BITS = {max_recv_mask_bits};\n")

        # Sort messages for consistent LUT generation (not strictly necessary for functionality
        # but good for diff stability and readability)
        sorted_telem_to_vitals = sorted(telem_to_vitals, key=lambda m: (m.get("mask_bits", 0), m['name']))

        f.write(f"const RecvPacketLUTEntry recvPacketLUT[] = {{\n")
        for msg in sorted_telem_to_vitals:
            mask_val = msg["mask"]
            mask_bits = msg["mask_bits"]
            name = msg["name"]
            byte_count = msg.get("byteCount")
            fields = msg.get("msgFields", [])
            
            f.write(f"    {{ // {name}\n")
            field_ptr = f"{name}_fields" if len(fields) > 0 else "NULL" # Fields for the telemetry packet
            f.write(f"        .fields = {field_ptr},\n")
            f.write(f"        .num_fields = {len(fields)},\n")
            f.write(f"        .mask_val = {mask_val},\n")
            f.write(f"        .mask_bits = {mask_bits},\n")
            f.write(f"        .packet_type = RECV_PACKET_TYPE_{'CUSTOM' if byte_count is CUSTOM else 'FIXED'},\n")
            f.write(f"        .callback_wrapper = on{name}_wrapper,\n") # Corrected: Use on<MsgName>_wrapper
            f.write("    },\n")
        f.write("};\n\n")
        # The LUT is no longer indexed by mask_val, so we need to iterate through it.
        # The size is still useful.
        f.write("const size_t recvPacketLUTSize = sizeof(recvPacketLUT) / sizeof(RecvPacketLUTEntry);\n")