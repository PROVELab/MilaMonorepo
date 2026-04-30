import os
from packetFormat import vitals_to_telem, telem_to_vitals, setPacketParameters, FIXED, CUSTOM
from parseFile import ACCESS, globalEnums, globalDefines
import math

nodeCount = maxFrameCnt = maxDataCnt = 0
def createPacketSendFiles(generated_code_dir, set_nodeCount, set_maxFrameCnt, set_maxDataCnt):
    global nodeCount, maxFrameCnt, maxDataCnt
    nodeCount, maxFrameCnt, maxDataCnt = set_nodeCount, set_maxFrameCnt, set_maxDataCnt
    setPacketParameters(nodeCount, maxFrameCnt, maxDataCnt)


    """
    Generates C source and header files for sending formatted telemetry packets.
    - vitalsPacketSendLUT.h/c: Contains lookup tables for packet message fields.
    """
    
    vitals_helper_dir = os.path.join(generated_code_dir, "..", "..", "src", "vitalsNode", "vitalsHelper")
    vitals_helper_dir = os.path.normpath(vitals_helper_dir)
    os.makedirs(vitals_helper_dir, exist_ok=True)

    header_path = os.path.join(vitals_helper_dir, "vitalsPacketSendLUT.h")
    source_path = os.path.join(vitals_helper_dir, "vitalsPacketSendLUT.c")

    # --- Generate Header File ---
    import math

    # Assuming parse_file_name is defined in your broader script context
    output_dir = f"genFor{parse_file_name}"
    os.makedirs(output_dir, exist_ok=True)

    header_path = os.path.join(output_dir, "VITALS_PACKET_SEND_LUT_H.h")
    mapping_path = os.path.join(output_dir, "mask_mappings.txt")

    # 1. Automatically assign masks and write the mapping file
    current_mask = 0
    with open(mapping_path, 'w') as map_file:
        map_file.write("Packet Name -> Assigned Mask\n")
        map_file.write("-" * 30 + "\n")
        
        for msg in vitals_to_telem:
            mask_bits = msg.get("mask_bits", 0)
            
            if mask_bits > 0:
                msg["mask"] = current_mask
                if current_mask >= (1 << mask_bits):
                    raise ValueError(f"Mask value {current_mask} overflows {mask_bits} mask_bits for packet {msg['name']}")
                    
                map_file.write(f"{msg['name']}: {current_mask} (0x{current_mask:X})\n")
                current_mask += 1
            else:
                msg["mask"] = 0 
                map_file.write(f"{msg['name']}: No mask (0 bits)\n")

    # 2. Generate the Header Code
    with open(header_path, 'w') as f:
        f.write("#ifndef VITALS_PACKET_SEND_LUT_H\n")
        f.write("#define VITALS_PACKET_SEND_LUT_H\n\n")
        f.write('#include "vitalsStructs.h"\n')
        f.write('#include <stddef.h>\n')
        f.write('#include <stdint.h>\n')
        f.write('#include "freertos/FreeRTOS.h"\n')
        f.write('#include "freertos/semphr.h"\n\n')
        
        # Reverted prototypes back to the cleaner version
        f.write("void sendPacketCore(const simpleDataPoint* fields, size_t numFields, const int* data, uint8_t* dataBuffer);\n")
        f.write("void sendPacketVariable(const simpleDataPoint* fields, size_t numFields, const int* data, const uint8_t* payload, const uint8_t payloadBytes);\n\n")

        for msg in vitals_to_telem:
            name = msg["name"]
            byte_count = msg.get("byteCount")
            mask_bits = msg.get("mask_bits", 0)
            mask_val = msg.get("mask", 0)
            
            fields = msg.get("msgFields", [])

            for field in fields:
                for attr in ("bits", "min", "max"):
                    val = getattr(field, attr, None)
                    if callable(val):
                        setattr(field, attr, val())
                
                if getattr(field, 'isEnum', False): 
                    try:
                        enum = next(entry for entry in globalEnums if entry.enum_name == field.name)
                    except StopIteration: 
                        raise ValueError(f"Enum {field.name} not found in globalEnums")
                    
                    field.min = min(entry.value_int for entry in enum.entries)
                    field.max = max(entry.value_int for entry in enum.entries)
                    field.bits = max(1, math.ceil(math.log2(int(field.max) + 1)))
                
                if getattr(field, 'max', None) is None:
                    field.max = (1 << field.bits) - 1

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
                        f.write(f"    args.mask = {mask_val}; // Auto-assigned\n")
                    f.write(f"    int32_t* data = (int32_t*) &args; \n")
                    f.write(f"    sendPacketVariable({field_ptr}, {total_fields}, data, args.payload, args.payloadBytes);\n")
                    f.write("}\n\n")
                else:
                    f.write(f"inline void send{name}Function(const uint8_t* payload, size_t payloadBytes) {{\n")
                    if mask_bits > 0:
                        f.write(f"    int32_t data[1] = {{{mask_val}}};\n")
                        f.write(f"    sendPacketVariable({field_ptr}, {total_fields}, data, payload, payloadBytes);\n")
                    else:
                        f.write(f"    sendPacketVariable({field_ptr}, {total_fields}, NULL, payload, payloadBytes);\n")
                    f.write("}\n\n")
                    
            elif byte_count is FIXED:
                if has_struct:
                    f.write(f"inline void send{name}Function({struct_name} args) {{\n")
                    if mask_bits > 0:
                        f.write(f"    args.mask = {mask_val}; // Auto-assigned\n")
                    f.write(f"    int32_t* data = (int32_t*) &args; \n")
                    f.write(f"    uint8_t dataBuffer[{num_bytes}] = {{0}};\n")
                    f.write(f"    sendPacketCore({field_ptr}, {total_fields}, data, dataBuffer);\n")
                    f.write("}\n\n")
                else:
                    f.write(f"inline void send{name}Function() {{\n")
                    f.write(f"    uint8_t dataBuffer[{num_bytes}] = {{0}};\n")
                    if mask_bits > 0:
                        f.write(f"    int32_t data[1] = {{{mask_val}}};\n")
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
            
            if byte_count is FIXED:
                name = msg["name"]
                fields = msg.get("msgFields", [])
                mask_bits = msg.get("mask_bits", 0)
                
                # +1 to include the mask as the first field
                total_fields = len(fields) + 1 
                
                f.write(f"// ----- {name} -----\n")
                f.write(f"const simpleDataPoint {name}_fields[{total_fields}] = {{\n")
                
                # Insert mask as the first element in the LUT
                mask_max = (1 << mask_bits) - 1
                f.write(f"    {{ .bits={mask_bits}, .min=0, .max={mask_max} }}, // Mask\n")
                
                # Insert the rest of the fields
                for field in fields:
                    f.write(f"    {{ .bits={field.bits}, .min={field.min}, .max={field.max} }},\n")
                f.write("};\n\n")