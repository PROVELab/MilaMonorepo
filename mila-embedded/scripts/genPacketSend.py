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
    with open(header_path, 'w') as f:
        f.write("#ifndef VITALS_PACKET_SEND_LUT_H\n")
        f.write("#define VITALS_PACKET_SEND_LUT_H\n\n")
        f.write('#include "vitalsStructs.h"\n')
        f.write('#include <stddef.h>\n')
        f.write('#include <stdint.h>\n\n')
        f.write("void sendPacketCore(const simpleDataPoint* fields, size_t numFields, "\
                "const int* data, uint8_t* tempData, size_t tempBytes);\n\n")
        
        # Note: Ensure void sendPacketCore(const simpleDataPoint* fields, size_t numFields, const int* data); 
        # is declared in vitalsStructs.h or added here.

        for msg in vitals_to_telem:
            name = msg["name"]
            byte_count = msg.get("byteCount")
            
            f.write(f"// ----- {name} -----\n")

            if byte_count is CUSTOM:
                # Gemini Todo 1 filled: Header takes uint8_t* array and size
                f.write(f"void send{name}Function(const uint8_t* data, size_t size);\n\n")

            elif byte_count is FIXED:
                fields = msg.get("msgFields", [])

                for field in fields:
                    for attr in ("bits", "min", "max"):
                        val = getattr(field, attr, None)
                        if callable(val):
                            setattr(field, attr, val())
                    # Modification 3: Fixed dot-notation on dict and moved to field
                    if getattr(field, 'isEnum', False): 
                        try:
                            enum = next(entry for entry in globalEnums if entry.enum_name == field.name)
                        except StopIteration:  # Safest practice to specifically catch StopIteration
                            raise ValueError(f"Enum {field.name} not found in globalEnums")
                        # 1. Get the min and max (assuming entry.bits holds the integer value)
                        field.min = min(entry.value_int for entry in enum.entries)
                        field.max = max(entry.value_int for entry in enum.entries)

                        # 2. Calculate bits (using max + 1 to account for 0-indexing)
                        # We use max(1, ...) to ensure we never return 0 bits.
                        field.bits = max(1, math.ceil(math.log2(int(field.max) + 1)))
                    if getattr(field, 'max', None) is None:
                        field.max = (1 << field.bits) - 1

                for field in fields:
                    assert(field.max <= 2147483647)
                    assert(field.min >= -2147483648)
                
                # Mask implementation logic
                mask_bits = msg.get("mask_bits", 0)
                mask_val = msg.get("mask", 0)
                
                # Calculate Array Sizes
                total_fields = len(fields) + 1 
                totalBits = mask_bits + sum(f.bits for f in fields)
                num_bytes = (totalBits + 7) // 8  # Ceiling division by 8 to get total bytes needed
                
                # Gemini Todo 2 filled: Write extern LUT and Inline Wrapper
                f.write(f"extern const simpleDataPoint {name}_fields[{total_fields}];\n")
                
                # Generate the arguments for the wrapper: int32_t d0, int32_t d1...
                args_str = ", ".join(f"int32_t d{i}" for i in range(len(fields)))
                
                # Prepend the hardcoded mask value to the data array initialization
                arr_elements = f"(int32_t){mask_val}U"
                if fields:
                    arr_elements += ", " + ", ".join(f"d{i}" for i in range(len(fields)))
                
                f.write(f"inline void send{name}Function({args_str}) {{\n")
                f.write(f"    int32_t data[{total_fields}] = {{{arr_elements}}};\n")
                f.write(f"    uint8_t tempData[{num_bytes}] = {{0}};\n") # Dynamically sized array
                f.write(f"    sendPacketCore({name}_fields, {total_fields}, data, tempData, {num_bytes});\n")
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