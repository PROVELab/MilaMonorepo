import os
from config.parseFile import parse_config, _parse_enums_and_defines, globalDefines, globalEnums, dataPoint_fields, CANFrame_fields
from genSensors import createSensors 
from genVitals import createVitals
from genTelemetry import createCommandRecords, createTelemetry, createTelemetryParserLUT, createTelemetryRecords, get_telem_path
from Lora_Msgs_And_Cmds.genPacketSend import createPacketSendFiles
from Lora_Msgs_And_Cmds.genCallbacks import generate_all_callbacks_and_skeletons
from Lora_Msgs_And_Cmds.processPacketFormat import preprocess_packets
from Lora_Msgs_And_Cmds.packetFormat import vitals_to_telem, telem_to_vitals
import math

import json
def pretty_print_vitals(vitals_nodes):  #useful for debugging
    print("Vitals Nodes:")
    for i, node in enumerate(vitals_nodes):
        print(f"\nNode {i + 1}:")
        for field in node:
            if field["type"] == "list" and isinstance(field["value"], list):
                print(f"  {field['name']} ({field['type']}):")
                for j, sub_field in enumerate(field["value"]):
                    if isinstance(sub_field, list):
                        print(f"    Sub-List {j + 1}:")
                        for k, item in enumerate(sub_field):
                            print(f"      {k + 1}. {json.dumps(item, indent=8)}")
                    else:
                        print(f"    {j + 1}. {json.dumps(sub_field, indent=6)}")
            else:
                print(f"  {field['name']} ({field['type']}): {field['value']}")

from genUtils import interactive_file_gen

if __name__ == "__main__":
    # Get the directory of the current script
    script_dir = os.path.dirname(os.path.abspath(__file__))

    # Define file names
    node_def_file_name = "simpleTest.def"
    enum_def_file_name = "enum.def"

    # Build absolute paths to the input files
    node_def_file_path = os.path.join(script_dir, "config", node_def_file_name)
    enum_def_file_path = os.path.join(script_dir, "config", enum_def_file_name)

    # Make directory for the generated code
    base_name = os.path.splitext(os.path.basename(node_def_file_name))[0]
    generated_code_dir = os.path.join(script_dir, f"generated_{base_name}")
    os.makedirs(generated_code_dir, exist_ok=True)

    # --- Pre-process Packet Formats ---
    print("Parsing enums and defines from:", enum_def_file_name)
    _parse_enums_and_defines(enum_def_file_path) # This populates globalEnums and globalDefines

    print("Parsing node definitions from:", node_def_file_name)
    (nodes, startingNodeID, missingIDs, frameCount, maxFrameCnt, maxDataCnt) = parse_config(node_def_file_path)
    print("Done parsing.")

    nodeCount = len(nodes)
    print("\n--- Parsed Configuration ---")
    print("Starting Node ID:", startingNodeID)
    print("Missing IDs:", missingIDs)
    print("Node Count:", nodeCount)
    print("Node Names:", [node.name for node in nodes])

    # Get vitals helper dir, where most vitals-related generated files will go
    vitals_gen_dir = os.path.join(script_dir, "..", "..", "src", "vitalsNode", "vitalsGen")
    vitals_gen_dir = os.path.normpath(vitals_gen_dir)
    os.makedirs(vitals_gen_dir, exist_ok=True)

    # Get vitals node dir, for files like user-editable callbacks
    vitals_node_dir = os.path.join(script_dir, "..", "..", "src", "vitalsNode")
    vitals_node_dir = os.path.normpath(vitals_node_dir)
    os.makedirs(vitals_node_dir, exist_ok=True)

    # New callbacks directory for C++
    vitals_callbacks_dir = os.path.join(vitals_node_dir, "callbacks")
    os.makedirs(vitals_callbacks_dir, exist_ok=True)

    # --- Pre-process Packet Formats (after parsing config) ---
    print("\nPre-processing packet formats...")
    preprocess_packets(nodes, maxFrameCnt, maxDataCnt)
    print("Done.")
    
    from Lora_Msgs_And_Cmds.mask_creation import assign_all_masks
    assign_all_masks(generated_code_dir)
    
    createSensors(nodes, script_dir, generated_code_dir, telem_to_vitals, globalEnums)
    createVitals(nodes, missingIDs, frameCount, globalDefines, vitals_gen_dir, len(vitals_to_telem))
    createTelemetry(nodes, "telemetryDashboard.csv", generated_code_dir, vitals_to_telem, globalDefines)
    
    Presentation_path = os.path.join(get_telem_path(), 'java', 'presentation')
    Lookup_path = os.path.join(get_telem_path(), 'java', 'lookup')
    Application_path = os.path.join(get_telem_path(), 'java', 'application')
    createTelemetryParserLUT(vitals_to_telem, Presentation_path)
    createTelemetryRecords(dataPoint_fields, CANFrame_fields, Lookup_path)
    
    createPacketSendFiles(generated_code_dir, vitals_gen_dir, vitals_node_dir, nodes, vitals_to_telem, telem_to_vitals, globalEnums)

    generate_all_callbacks_and_skeletons(vitals_to_telem, telem_to_vitals, globalEnums, nodes, Presentation_path, Application_path, vitals_callbacks_dir)

    print("Done.")
