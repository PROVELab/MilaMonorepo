import os
from typing import Any

from config.parseFile import (
    Node,
    ParsedFields,
    ACCESS,
    expression_to_int,
    is_critical_datapoint,
)

def createVitalsGen(nodes: list[Node], fields: ParsedFields, vitals_dir: str) -> None:

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
                critical_data_points = [
                    dataPoint for dataPoint in ACCESS(frame, "dataInfo")["value"]
                    if is_critical_datapoint(dataPoint)
                ]

                critical_buffer_name = f"n{nodeIndex}f{frameIndex}CriticalData"

                if critical_data_points:
                    f.write(
                        f"int32_t {critical_buffer_name}[{len(critical_data_points)}][pointsPerData]={{"
                    )
                    r8_values = [
                        f"R8({expression_to_int(ACCESS(dataPoint, 'startingValue')['value'])})"
                        for dataPoint in critical_data_points
                    ]
                    f.write(",".join(r8_values))
                    f.write("};\n\n")

                    f.write(
                        f"critical_dataPoint n{nodeIndex}f{frameIndex}CriticalDPs "
                        f"[{len(critical_data_points)}]={{\n"
                    )
                    for criticalIndex, dataPoint in enumerate(critical_data_points):
                        critical_fields = []
                        for field in fields.critical_dataPoint_fields:
                            if field["name"] == "data":
                                critical_fields.append(f".data={critical_buffer_name}[{criticalIndex}]")
                            else:
                                evaluated_value = expression_to_int(ACCESS(dataPoint, field['name'])['value'])
                                critical_fields.append(f".{field['name']}={evaluated_value}")
                        f.write("    {" + ", ".join(critical_fields) + "},\n")
                    f.write("};\n\n")

                f.write(f"dataPoint n{nodeIndex}f{frameIndex}DPs [{num_data_points}]={{\n")
                critical_index = 0
                for dataPoint in ACCESS(frame, "dataInfo")["value"]:
                    dp_fields = []
                    for field in fields.dataPoint_fields:
                        if "vitals" in field["node"] and field['name'] != 'enum':
                            if field["name"] == "criticalStructPtr":
                                if is_critical_datapoint(dataPoint):
                                    dp_fields.append(
                                        f".criticalStructPtr=&n{nodeIndex}f{frameIndex}"
                                        f"CriticalDPs[{critical_index}]"
                                    )
                                    critical_index += 1
                                else:
                                    dp_fields.append(".criticalStructPtr=NULL")
                            else:
                                evaluated_value = expression_to_int(ACCESS(dataPoint, field['name'])['value'])
                                dp_fields.append(f".{field['name']}={evaluated_value}")
                    f.write("    {" + ", ".join(dp_fields) + "},\n")
                f.write("};\n\n")

            #CANFrames
            f.write(f"CANFrame n{nodeIndex}[{ACCESS(node, 'numFrames')['value']}]={{\n")
            for frameIndex, frame in enumerate(ACCESS(node, "CANFrames")["value"]) :
                frame_fields = [f".{field['name']}={ACCESS(frame, field['name'])['value']}"
                                for field in fields.CANFrame_fields if "vitals" in field["node"]]
                f.write(f"    {{{', '.join(frame_fields)}, .dataInfo=n{nodeIndex}f{frameIndex}DPs}},\n")
            f.write("};\n\n")

        #vitalsNode nodes
        f.write("// vitalsData *nodes;\n")
        f.write(f"vitalsNode nodes [{len(nodes)}]={{\n")
        for nodeIndex, node_info in enumerate(nodes):
            node = node_info.vitals_data
            NODE_fields = [f".{field['name']}={ACCESS(node, field['name'])['value']}"
                            for field in Node.vitalsNode_fields if field["name"] not in {"CANFrames"}]
                            #^ Exclude CANFrames, as that is handled specially below
            f.write(f"    {{{', '.join(NODE_fields)}, .CANFrames=n{nodeIndex}}},\n")
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

        # Critical dataPoint struct definition
        f.write("typedef struct {\n")
        for field in fields.critical_dataPoint_fields:
            f.write("    ")
            if field["name"] == "data":
                f.write("int32_t *data; /* pointsPerData samples for this critical datapoint */\n")
            else:
                c_type = "bool" if field['type'] == "boolean" else field['type']
                if field['Atomic'] == True:
                    f.write(f"ATOMIC({c_type}) {field['name']};\n")
                else:
                    f.write(f"{c_type} {field['name']};\n")
        f.write("} critical_dataPoint;\n\n")

        # DataPoint struct definition
        f.write("typedef struct {\n")
        for field in fields.dataPoint_fields:
            if field['name'] != 'enum' and "vitals" in field['node']: # 'enum' is a pseudo-field for the generator
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
        for field in fields.CANFrame_fields:
            #explicitly write the "array" fields
            if field['name'] == "dataInfo":
                f.write("    dataPoint *dataInfo; /* Replaced list with dataPoint pointer */\n")
            elif field['name'] == "CANFrames":
                f.write("    CANFrame *CANFrames; /* Replaced list with CANFrame pointer */\n")
            else:
                if("vitals" in field['node']):
                    # Use 'bool' for boolean types
                    c_type = "bool" if field['type'] == "boolean" else field['type']
                    f.write(f"    {c_type} {field['name']};\n")
        
        f.write("} CANFrame;\n\n")

        # VitalsNode struct definition
        f.write("typedef struct {\n")
        for field in Node.vitalsNode_fields:
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
        f.write("extern vitalsNode nodes[numberOfNodes];\n\n")
        f.write("#endif\n")
        f.close()

def createVitals(
    nodes: list[Node],
    fields: ParsedFields,
    vitals_to_telem: list[dict[str, Any]],
    telem_to_vitals: list[dict[str, Any]],
    vitals_gen_dir: str,
    vitals_callbacks_dir: str,

) -> None:
    from Lora_Msgs_And_Cmds.genPacketSend import createPacketSendFiles
    from Lora_Msgs_And_Cmds.genVitalsCallbacks import generate_vitals_callback_skeletons

    createVitalsGen(nodes, fields, vitals_gen_dir)
    createPacketSendFiles(vitals_gen_dir, nodes, vitals_to_telem, telem_to_vitals)
    generate_vitals_callback_skeletons(telem_to_vitals, vitals_callbacks_dir)
