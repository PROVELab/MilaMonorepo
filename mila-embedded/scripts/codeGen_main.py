import os
from parseFile import parse_config, _parse_enums_and_defines, globalDefines, globalEnums
from genSensors import createSensors 
from genVitals import createVitals
from genTelemetry import createTelemetry 
from genPacketSend import createPacketSendFiles
from processPacketFormat import preprocess_packets
from packetFormat import vitals_to_telem, telem_to_vitals

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

if __name__ == "__main__":
    # Get the directory of the current script
    script_dir = os.path.dirname(os.path.abspath(__file__))

    # Define file names
    node_def_file_name = "simpleTest.def"
    enum_def_file_name = "enum.def"

    # Build absolute paths to the input files
    node_def_file_path = os.path.join(script_dir, node_def_file_name)
    enum_def_file_path = os.path.join(script_dir, enum_def_file_name)

    # Make directory for the generated code
    base_name = os.path.splitext(os.path.basename(node_def_file_name))[0]
    generated_code_dir = os.path.join(script_dir, f"generated_{base_name}")
    os.makedirs(generated_code_dir, exist_ok=True)

    # --- Pre-process Packet Formats ---
    print("Parsing enums and defines from:", enum_def_file_name)
    _parse_enums_and_defines(enum_def_file_path) # This populates globalEnums and globalDefines

    print("Parsing node definitions from:", node_def_file_name)
    (vitalsNodes, nodeNames, boardTypes, dataNames, numData, nodeIds,
     startingNodeID, missingIDs, nodeCount, frameCount, maxFrameCnt, maxDataCnt) = parse_config(node_def_file_path)
    print("Done parsing.")

    print("\n--- Parsed Configuration ---")
    print("Starting Node ID:", startingNodeID)
    print("Missing IDs:", missingIDs)
    print("Node Count:", nodeCount)
    print("Node Names:", nodeNames)

    # Get vitals helper dir, where most vitals-related generated files will go
    vitals_helper_dir = os.path.join(script_dir, "..", "src", "vitalsNode", "vitalsHelper")
    vitals_helper_dir = os.path.normpath(vitals_helper_dir)
    os.makedirs(vitals_helper_dir, exist_ok=True)

    # Get vitals node dir, for files like user-editable callbacks
    vitals_node_dir = os.path.join(script_dir, "..", "src", "vitalsNode")
    vitals_node_dir = os.path.normpath(vitals_node_dir)
    os.makedirs(vitals_node_dir, exist_ok=True)

    # --- Pre-process Packet Formats (after parsing config) ---
    print("\nPre-processing packet formats...")
    preprocess_packets(nodeCount, maxFrameCnt, maxDataCnt)
    print("Done.")

    createSensors(vitalsNodes, nodeNames, boardTypes, nodeIds, dataNames, numData, script_dir, generated_code_dir, telem_to_vitals, globalEnums)
    createVitals(vitalsNodes, nodeNames, nodeIds, missingIDs, nodeCount, frameCount, globalDefines, vitals_helper_dir)
    createTelemetry(vitalsNodes, "telemetryDashboard.csv", generated_code_dir, nodeNames, dataNames)

    createPacketSendFiles(generated_code_dir, vitals_helper_dir, vitals_node_dir, nodeNames, nodeIds, vitals_to_telem, telem_to_vitals, globalEnums)
    print("Done.")
