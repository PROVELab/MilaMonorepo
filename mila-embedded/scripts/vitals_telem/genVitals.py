import os
from config.parseFile import dataPoint_fields, CANFrame_fields, vitalsNode_fields, globalDefine, ACCESS, expression_to_int
from constantGen import writeConstants
from genTelemetry import get_telem_path


def createVitals(nodes, missingIDs, frameCount, globalDefines, vitals_dir, num_vitals_to_telem_packets):

    # 4. Create the directory tree if it doesn't exist
    os.makedirs(vitals_dir, exist_ok=True)

    numMissingIDs=0
    # Define the file path (this joins the path with the filename)
    file_path = os.path.join(vitals_dir, 'vitalsStaticDec.c')
    print("generating\n")
    with open(file_path, 'w') as f:
        f.write(
            '#include <stdio.h>\n'
            '#include <stdint.h>\n'
            '#include "vitalsStructs.h"\n'
            '\n'
            '#define R8(x) {x,x,x,x,x,x,x,x}\n'
        )

        for nodeIndex, node_info in enumerate(nodes):
            node = node_info.vitals_data
            f.write(f"// Node {nodeIndex}: {node_info.name}\n")

            #dataPoint structs
            for frameIndex, frame in enumerate(ACCESS(node, "CANFrames")["value"]):
                num_data_points = ACCESS(frame, "numData")["value"]
                f.write(f"dataPoint n{nodeIndex}f{frameIndex}DPs [{num_data_points}]={{\n")
                for dataPoint in ACCESS(frame, "dataInfo")["value"]:
                    fields = []
                    for field in dataPoint_fields:
                        if "vitals" in field["node"] and field['name'] != 'enum':
                            evaluated_value = expression_to_int(ACCESS(dataPoint, field['name'])['value'])
                            fields.append(f".{field['name']}={evaluated_value}")
                    f.write("    {" + ", ".join(fields) + "},\n")
                f.write("};\n\n")

            #arrays of datapoint structs
            for frameIndex, frame in enumerate(ACCESS(node, "CANFrames")["value"]):
                num_data_points = ACCESS(frame, "numData")["value"]
                f.write(f"int32_t n{nodeIndex}f{frameIndex}Data[{num_data_points}][8]={{")
                r8_values = [f"R8({expression_to_int(ACCESS(dataPoint, 'startingValue')['value'])})"
                              for dataPoint in ACCESS(frame, "dataInfo")["value"]
                              if any("vitals" in field["node"] for field in dataPoint_fields)]
                f.write(",".join(r8_values))
                f.write("};\n\n")

            #CANFrames
            f.write(f"CANFrame n{nodeIndex}[{ACCESS(node, 'numFrames')['value']}]={{\n")
            for frameIndex, frame in enumerate(ACCESS(node, "CANFrames")["value"]):
                frame_fields = [f".{field['name']}={ACCESS(frame, field['name'])['value']}"
                                for field in CANFrame_fields if "vitals" in field["node"]]
                f.write(f"    {{{', '.join(frame_fields)}, .data=n{nodeIndex}f{frameIndex}"
                    f"Data , .dataInfo=n{nodeIndex}f{frameIndex}DPs}},\n")
            f.write("};\n\n")

        #vitalsNode nodes
        f.write("// vitalsData *nodes;\n")
        f.write(f"vitalsNode nodes [{len(nodes)}]={{\n")
        for nodeIndex, node_info in enumerate(nodes):
            node = node_info.vitals_data
            NODE_fields = [f".{field['name']}={ACCESS(node, field['name'])['value']}"
                            for field in vitalsNode_fields if field["name"] not in {"CANFrames"}]
                            #^ Exclude CANFrames, as that is handled specially below
            f.write(f"    {{{', '.join(NODE_fields)}, .CANFrames=n{nodeIndex}}},\n")
        f.write("};\n")

        f.write("int16_t missingIDs[]={")
        i=0
        first=1
        while(i<len(missingIDs)):            
            if(first) :
                first=0
                f.write(f"{missingIDs[i]}")
            else:
                f.write(f", {missingIDs[i]}")
            numMissingIDs+=1
            i+=1
        f.write("};\n")
        f.close()

        #make the vitalsStruct.h file:
    structs_file_path = os.path.join(vitals_dir, 'vitalsStructs.h')
    with open(structs_file_path, "w") as f:
        f.write("#ifndef VITALS_STRUCTS_H\n")
        f.write("#define VITALS_STRUCTS_H\n\n")
        f.write("#include <stdio.h>\n")
        f.write("#include <stdint.h>\n")
        f.write('#include <stdatomic.h>\n')
        f.write('#include <stddef.h> // For offsetof\n')
        f.write("#include \"../../programConstants.h\"\n")
        f.write('#include "pecan/pecan.h" // For simpleDataPoint\n#include <stdbool.h> // For bool type\n\n')
        f.write("#define R8(x) {x,x,x,x,x,x,x,x}\n\n")

        # Add a cross-compatible static assert macro
        f.write("""#if defined(__cplusplus)
#include <atomic>
#define ATOMIC(X) std::atomic< X >
#define STATIC_ASSERT(cond, msg) static_assert((cond), msg)
#else
#define ATOMIC(X) _Atomic X
#define STATIC_ASSERT(cond, msg) _Static_assert((cond), msg)
#endif

""")

        # DataPoint struct definition
        f.write("typedef struct {\n")
        for field in dataPoint_fields:
            if field['name'] != 'enum': # 'enum' is a pseudo-field for the generator
                f.write("    ")
                c_type = "bool" if field['type'] == "boolean" else field['type']
                if field['Atomic'] == True:
                    f.write(f"ATOMIC({c_type}) {field['name']};\n")
                else:
                    f.write(f"{c_type} {field['name']};\n")
        f.write("} dataPoint;\n\n")

        # This ensures that dataPoint can be safely cast to simpleDataPoint for pecan_unpack.
        f.write("// This ensures that dataPoint can be safely cast to simpleDataPoint for pecan_unpack.\n")
        f.write("STATIC_ASSERT(offsetof(dataPoint, min) == offsetof(simpleDataPoint, min), \"min offset mismatch\");\n")
        f.write("STATIC_ASSERT(offsetof(dataPoint, max) == offsetof(simpleDataPoint, max), \"max offset mismatch\");\n")
        f.write("STATIC_ASSERT(offsetof(dataPoint, bits) == offsetof(simpleDataPoint, bits), \"bits offset mismatch\");\n\n")

        # CANFrame struct definition
        f.write("typedef struct {\n")
        for field in CANFrame_fields:
            #explicitly write the "array" fields
            if field['name'] == "dataInfo":
                f.write("    dataPoint *dataInfo; /* Replaced list with dataPoint pointer */\n")
            elif field['name'] == "CANFrames":
                f.write("    CANFrame *CANFrames; /* Replaced list with CANFrame pointer */\n")
            else:
                # Use 'bool' for boolean types
                c_type = "bool" if field['type'] == "boolean" else field['type']
                f.write(f"    {c_type} {field['name']};\n")
        
        # Manually insert the custom 'data' field
        f.write("    int32_t (*data)[pointsPerData]; /* Init to [data points per data = 8] [numData for this frame] */\n")
        f.write("} CANFrame;\n\n")

        # VitalsNode struct definition
        f.write("typedef struct {\n")
        for field in vitalsNode_fields:
            if field['name'] == "CANFrames":
                f.write("    CANFrame *CANFrames; \n")  #Write CANFrames field manually as pointer
            else:
                # For other fields, write them as usual
                f.write("    ")
                if(field['Atomic'] == True) :
                    f.write(f"ATOMIC({field['type']}) {field['name']};\n")
                else:
                    f.write(f"{field['type']} {field['name']};\n")
        f.write("} vitalsNode;\n\n")

        # End of header guards
        f.write("extern vitalsNode nodes[numberOfNodes];\nextern int16_t missingIDs[];\n\n")
        f.write("#endif\n")
        f.close()

    #generate constants files
    node_ids = [node.id for node in nodes]
    minId = min(node_ids) if node_ids else 0
    # 1. Anchor to the current script's directory
    script_dir = os.path.dirname(os.path.abspath(__file__))

    # --- 1. C Constants (Target: src/programConstants.h) ---
    # Go up one level (..) -> into src
    c_constants_dir = os.path.join(script_dir, "..", "..", "src")
    c_constants_dir = os.path.normpath(c_constants_dir)
    os.makedirs(c_constants_dir, exist_ok=True)

    constants_file_path = os.path.join(c_constants_dir, 'programConstants.h')

    # Write C Constants
    writeConstants("c", constants_file_path, minId, numMissingIDs, len(nodes), frameCount, globalDefines, missingIDs, num_vitals_to_telem_packets)


    # --- 2. Java Constants (Target: ../../telem-dashboard/src/main/java/Constants.java) ---
    # Go up two levels (../..) -> into telem-dashboard -> src -> main -> java

    constants_file_path = os.path.join(get_telem_path(), 'java', 'util', 'Constants.java')

    # Write Java Constants
    writeConstants("java", constants_file_path, minId, numMissingIDs, len(nodes), frameCount, globalDefines, missingIDs, num_vitals_to_telem_packets)

# Note: The ACCESS helper is also defined here to allow local field lookup.
def ACCESS(fields, name):
    return next(field for field in fields if field["name"] == name)
