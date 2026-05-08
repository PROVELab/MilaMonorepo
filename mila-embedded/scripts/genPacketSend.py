import os
from packetFormat import FIXED, CUSTOM
from parseFile import ACCESS, globalEnums, globalDefines
import math

def _assign_prefix_free_masks(packet_list, map_file, packet_direction_name):
    """
    Assigns unique, prefix-free masks to a list of packets using a canonical
    Huffman-like algorithm and writes the assignments to a mapping file.
    This function modifies the packet dictionaries in-place by adding "mask" keys.
    """
    # All packets must have a mask > 0 bits. This is enforced by processPacketFormat.py
    if any(msg.get("mask_bits", 0) == 0 for msg in packet_list):
        unmasked_names = [m['name'] for m in packet_list if m.get("mask_bits", 0) == 0]
        raise ValueError(f"FATAL: The following {packet_direction_name} packets have a 0-bit mask, which is not allowed: {unmasked_names}")

    # Use canonical Huffman-like algorithm to assign prefix-free codes.
    # Sort by mask length ascending, then by name for stable order.
    sorted_msgs = sorted(packet_list, key=lambda m: (m.get("mask_bits", 0), m['name']))

    next_code = 0
    current_len = 0
    if sorted_msgs:
        current_len = sorted_msgs[0].get("mask_bits", 0)

    for msg in sorted_msgs:
        mask_len = msg.get("mask_bits", 0)
        if mask_len > current_len:
            next_code <<= (mask_len - current_len)
            current_len = mask_len
        if len(bin(next_code)[2:]) > mask_len:
            raise ValueError(f"Not enough mask space for all {packet_direction_name} packets. Failed at packet '{msg['name']}'. Ran out of codes of length {mask_len}.")
        msg["mask"] = next_code
        map_file.write(f"{msg['name']} ({mask_len} bits): {next_code} (0b{next_code:0{mask_len}b})\n")
        next_code += 1

def createPacketSendFiles(generated_code_dir, vitals_helper_dir, vitals_node_dir, nodeNames, nodeIds, vitals_to_telem, telem_to_vitals, globalEnums):

    """
    Generates C source and header files for sending formatted telemetry packets.
    - vitalsPacketSendLUT.h/c: Contains lookup tables for packet message fields.
    """

    header_path = os.path.join(vitals_helper_dir, "vitalsPacketSendLUT.h")
    source_path = os.path.join(vitals_helper_dir, "vitalsPacketSendLUT.cpp")
    recv_header_path = os.path.join(vitals_helper_dir, "vitalsPacketRecvLUT.h")
    recv_source_path = os.path.join(vitals_helper_dir, "vitalsPacketRecvLUT.cpp")
    recv_callbacks_path = os.path.join(generated_code_dir, "vitalsRecvCallbacks.cpp")
    mapping_path = os.path.join(generated_code_dir, "mask_mappings.txt")

    node_id_map = {name: id for name, id in zip(nodeNames, nodeIds)}

    # --- Assign CAN-level masks for forwarded packets ---
    forwarded_packets_by_node = {}
    for msg in telem_to_vitals:
        target_node = msg.get("targetNode")
        if target_node and target_node != "vitals":
            if target_node not in forwarded_packets_by_node:
                forwarded_packets_by_node[target_node] = []
            forwarded_packets_by_node[target_node].append(msg)

    for node, msgs in forwarded_packets_by_node.items():
        if len(msgs) > 1:
            can_mask_bits = math.ceil(math.log2(len(msgs)))
            for i, msg in enumerate(msgs):
                msg['can_mask'] = i
                msg['can_mask_bits'] = can_mask_bits
        else:
            # If only one message for this target, no CAN mask is needed.
            for msg in msgs:
                msg['can_mask'] = 0
                msg['can_mask_bits'] = 0
        
        # Sanity check packet sizes
        for msg in msgs:
            if msg.get("byteCount") is FIXED:
                total_bits = msg.get('can_mask_bits', 0) + sum(f.bits for f in msg.get("msgFields", []))
                if total_bits > 64:
                    raise ValueError(f"Forwarded packet '{msg['name']}' for target '{node}' is too large for a CAN frame. It requires {total_bits} bits but max is 64.")

    # 1. Automatically assign masks and write the mapping file
    with open(mapping_path, 'w') as map_file:
        map_file.write("--- Vitals to Telem ---\n")
        map_file.write("Packet Name -> Assigned Mask\n\n")
        _assign_prefix_free_masks(vitals_to_telem, map_file, "vitals-to-telem")

        map_file.write("\n\n--- Telem to Vitals ---\n")
        map_file.write("Packet Name -> Assigned Mask\n\n")
        _assign_prefix_free_masks(telem_to_vitals, map_file, "telem-to-vitals")

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
        f.write('#include "pecan/pecan.h"\n')
        f.write('#include <stddef.h>\n')
        f.write('#include <stdint.h>\n')
        f.write('#include "freertos/FreeRTOS.h"\n')
        f.write('#include "freertos/semphr.h"\n\n')
        
        # Reverted prototypes back to the cleaner version
        f.write("void sendPacketCore(const simpleDataPoint* fields, size_t numFields, const int32_t* data, uint8_t* dataBuffer);\n")
        f.write("void sendPacketVariable(const simpleDataPoint* fields, size_t numFields, const int32_t* data, const uint8_t* payload, const uint8_t payloadBytes);\n\n")

        for msg in vitals_to_telem:
            name = msg["name"]
            byte_count = msg.get("byteCount")
            mask_bits = msg.get("mask_bits", 0)
            mask_val = msg.get("mask", 0)
            
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

            struct_name = f"send{name}Args"

            if has_struct:
                f.write(f"typedef struct __attribute__((packed)) {struct_name}{{\n")
                
                # Inject the mask into the struct so it gets picked up by the pointer cast
                if mask_bits > 0:
                    f.write(f"    int32_t mask;\n")
                    
                for field in fields:
                    f.write(f"    int32_t {field.name};\n")
                
                if byte_count is CUSTOM:
                    f.write("    uint8_t* payload;\n")
                    f.write("    size_t payloadBytes;\n")
                f.write(f"}} {struct_name};\n\n")

            field_ptr = f"{name}_fields" if total_fields > 0 else "NULL"
            if total_fields > 0:
                f.write(f"extern const simpleDataPoint {name}_fields[{total_fields}];\n")

            # Function Generation based on CUSTOM or FIXED
            if byte_count is CUSTOM:
                if has_struct:
                    f.write(f"inline void send{name}Function({struct_name} args) {{\n")
                    if mask_bits > 0:
                        f.write(f"    args.mask = (int32_t){mask_val}; // Auto-assigned\n")
                    f.write(f"    const int32_t* data = (const int32_t*) &args; \n")
                    f.write(f"    sendPacketVariable({field_ptr}, {total_fields}, data, args.payload, args.payloadBytes);\n")
                    f.write("}\n\n")
                else:
                    f.write(f"inline void send{name}Function(const uint8_t* payload, size_t payloadBytes) {{\n")
                    if mask_bits > 0:
                        f.write(f"    int32_t data[1] = {{(int32_t){mask_val}}};\n")
                        f.write(f"    sendPacketVariable({field_ptr}, {total_fields}, data, payload, payloadBytes);\n")
                    else:
                        f.write(f"    sendPacketVariable({field_ptr}, {total_fields}, NULL, payload, payloadBytes);\n")
                    f.write("}\n\n")
                    
            elif byte_count is FIXED:
                if has_struct:
                    f.write(f"inline void send{name}Function({struct_name} args) {{\n")
                    if mask_bits > 0:
                        f.write(f"    args.mask = (int32_t){mask_val}; // Auto-assigned\n")
                    f.write(f"    const int32_t* data = (const int32_t*) &args; \n")
                    f.write(f"    uint8_t dataBuffer[{num_bytes}] = {{0}};\n")
                    f.write(f"    sendPacketCore({field_ptr}, {total_fields}, data, dataBuffer);\n")
                    f.write("}\n\n")
                else:
                    f.write(f"inline void send{name}Function() {{\n")
                    f.write(f"    uint8_t dataBuffer[{num_bytes}] = {{0}};\n")
                    if mask_bits > 0:
                        f.write(f"    int32_t data[1] = {{(int32_t){mask_val}}};\n")
                        f.write(f"    sendPacketCore({field_ptr}, {total_fields}, data, dataBuffer);\n")
                    else:
                        f.write(f"    sendPacketCore({field_ptr}, {total_fields}, NULL, dataBuffer);\n")
                    f.write("}\n\n")
            else:
                raise ValueError("byte count is not an expected object")
            
        f.write("#endif // VITALS_PACKET_SEND_LUT_H\n")

    # --- Generate Source File ---
    with open(source_path, 'w') as f:
        f.write('#include "vitalsPacketSendLUT.h"\n\n')

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
                    f.write(f"    {{ .min=0, .max={mask_max}, .bits={mask_bits} }}, // Mask\n")

                # Insert the rest of the fields
                for field in fields:
                    f.write(f"    {{ .min={field.min}, .max={field.max}, .bits={field.bits} }},\n")
                f.write("};\n\n")

    # --- Generate Recv LUTs ---
    with open(recv_header_path, 'w') as f:
        f.write("#ifndef VITALS_PACKET_RECV_LUT_H\n")
        f.write("#define VITALS_PACKET_RECV_LUT_H\n\n")
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
                f.write(f"typedef struct __attribute__((packed)) {{\n")
                if len(fields) > 0:
                    for field in fields:
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
        f.write(f"extern const size_t recvPacketLUTSize;\n\n")
        f.write("#endif // VITALS_PACKET_RECV_LUT_H\n")

    with open(recv_source_path, 'w') as f:
        f.write('#include "vitalsPacketRecvLUT.h"\n')
        f.write('#include "../vitalsRecvData.h" // Ensure this path is correct\n')
        f.write('#include "pecan/pecan.h" // For sendPacket, combinedID, etc.\n')
        f.write('#include "../../programConstants.h"\n')
        f.write('#include "esp_log.h"\n')
        f.write('#include <string.h>\n\n')
        f.write('static const char* TAG = "VitalsRecvLUT";\n')

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
                    f.write(f"    {{ .min={field.min}, .max={field.max}, .bits={field.bits} }},\n")
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

            has_struct = len(fields) > 0 or byte_count is CUSTOM

            # Unpack fields into args struct
            if has_struct:
                f.write(f"    {struct_name} args;\n")
                if fields:
                    f.write(f"    int32_t* dest_ptr = (int32_t*)&args;\n")
                    f.write(f"    for (int i = 0; i < {len(fields)}; ++i) {{\n")
                    f.write(f"        pecan_unpack(&dest_ptr[i], raw_packet, &{name}_fields[i], bitIndex);\n")
                    f.write(f"    }}\n")

            # Forwarding logic
            if target_node != "vitals":
                node_id = node_id_map.get(target_node)
                if node_id is None:
                    for enum_def in globalEnums:
                        for entry in enum_def.entries:
                            if entry.name == target_node:
                                node_id = entry.value_int
                                break
                        if node_id is not None:
                            break
                if node_id is None:
                    raise ValueError(f"Target node '{target_node}' for packet '{name}' not found in node list or global enums.")

                can_mask = msg.get('can_mask', 0)
                can_mask_bits = msg.get('can_mask_bits', 0)
                
                f.write(f"\n    // This packet is forwarded to the target node '{target_node}'.\n")
                
                field_ptr = f"{name}_fields" if len(fields) > 0 else "NULL"
                num_fields = len(fields)
                packet_type_str = f"RECV_PACKET_TYPE_{'CUSTOM' if byte_count is CUSTOM else 'FIXED'}"
                args_ptr_str = "&args" if has_struct else "NULL"

                f.write(f"    forwardCANPacket({node_id}, {can_mask}, {can_mask_bits}, {field_ptr}, {num_fields}, {packet_type_str}, {args_ptr_str}, raw_packet, packet_len, bitIndex);\n")

            # Call user callback and return
            if has_struct:
                if byte_count is CUSTOM:
                    # The python generator asserts that CUSTOM packet headers are byte-aligned.
                    f.write(f"    size_t fixed_bytes = *bitIndex / 8;\n")
                    f.write(f"    if (packet_len > fixed_bytes) {{\n")
                    f.write(f"        args.payload = raw_packet + fixed_bytes;\n")
                    f.write(f"        args.max_payload_size = packet_len - fixed_bytes;\n")
                    f.write(f"    }} else {{\n")
                    f.write(f"        args.payload = NULL;\n")
                    f.write(f"        args.max_payload_size = 0;\n")
                    f.write(f"    }}\n")
                    f.write(f"    return on{name}(args);\n")
                else: # FIXED
                    f.write(f"    on{name}(args);\n")
                    f.write(f"    return 0; // FIXED packets don't consume payload\n")
            else: # No fields
                f.write(f"    on{name}();\n")
                f.write(f"    return 0; // FIXED packets don't consume payload\n")

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
            f.write(f"        .packet_type = RECV_PACKET_TYPE_{'CUSTOM' if byte_count is CUSTOM else 'FIXED'},\n")
            f.write(f"        .mask_val = {mask_val},\n")
            f.write(f"        .mask_bits = {mask_bits},\n")
            f.write(f"        .callback_wrapper = on{name}_wrapper,\n")
            f.write("    },\n")
        f.write("};\n\n")
        # The LUT is no longer indexed by mask_val, so we need to iterate through it.
        # The size is still useful.
        f.write("const size_t recvPacketLUTSize = sizeof(recvPacketLUT) / sizeof(RecvPacketLUTEntry);\n")

    # --- Generate Recv Callbacks Stubs (for user to implement) ---
    with open(recv_callbacks_path, 'w') as f:
        f.write('#include "vitalsHelper/vitalsPacketRecvLUT.h"\n')
        f.write('// #include "vitals/vitals.h" // No longer needed here, forwarding handled in LUT\n')
        f.write('#include "esp_log.h"\n\n')
        f.write('static const char* TAG = "VitalsRecvCallbacks";\n\n')
        f.write("/**\n * @brief These are auto-generated stub implementations for the packet receiver callbacks.\n"
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
            log_fmt = ""
            if fields:
                for field in fields:
                    log_fmt += f" {field.name}: %d"
                    log_args.append(f"args.{field.name}")
            
            log_prefix = f'Callback on{name} called'
            if target_node != "vitals":
                log_prefix += f" (packet was forwarded to {target_node} by Vitals)"

            f.write(f'    ESP_LOGI(TAG, "{log_prefix}.{log_fmt}"{", " + ", ".join(log_args) if log_args else ""});\n')
            f.write(f"    // TODO: Implement logic for {name}\n")

            if byte_count is CUSTOM:
                 f.write("    // Access payload via args.payload, with max size args.max_payload_size\n")
                 f.write("    // Example: ESP_LOG_BUFFER_HEX(TAG, args.payload, args.max_payload_size);\n")
                 f.write("    return 0; // Return number of payload bytes consumed\n")

            f.write("}\n\n")