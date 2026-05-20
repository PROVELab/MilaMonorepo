import os
from parseFile import parse_config, _parse_enums_and_defines, globalDefines, globalEnums, dataPoint_fields, CANFrame_fields
from genSensors import createSensors 
from genVitals import createVitals
from genTelemetry import createTelemetry, createTelemetryParser, createTelemetryRecords, get_telem_path
from genPacketSend import createPacketSendFiles, _assign_prefix_free_masks
from genCallbacks import generate_cpp_callbacks, generate_java_visitor_dispatcher, generate_single_java_callback_skeleton
from genCANCallbacks import create_can_frame_callbacks
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

from genUtils import interactive_file_gen

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

    # New callbacks directory for C++
    vitals_callbacks_dir = os.path.join(vitals_node_dir, "callbacks")
    os.makedirs(vitals_callbacks_dir, exist_ok=True)

    # --- Pre-process Packet Formats (after parsing config) ---
    print("\nPre-processing packet formats...")
    preprocess_packets(nodeCount, maxFrameCnt, maxDataCnt)
    print("Done.")
    
    # --- Assign masks ---
    mapping_path = os.path.join(generated_code_dir, "mask_mappings.txt")
    with open(mapping_path, 'w') as map_file:
        map_file.write("--- Vitals to Telem ---\n")
        map_file.write("Packet Name -> Assigned Mask\n\n")

        # Add packet_idx for Java generator before sorting for mask assignment
        for i, packet in enumerate(vitals_to_telem):
            packet['packet_idx'] = i

        _assign_prefix_free_masks(vitals_to_telem, map_file, "vitals-to-telem")
    
        map_file.write("\n\n--- Telem to Vitals ---\n")
        map_file.write("Packet Name -> Assigned Mask\n\n")
        _assign_prefix_free_masks(telem_to_vitals, map_file, "telem-to-vitals")
    
    createSensors(vitalsNodes, nodeNames, boardTypes, nodeIds, dataNames, numData, script_dir, generated_code_dir, telem_to_vitals, globalEnums)
    createVitals(vitalsNodes, nodeNames, nodeIds, missingIDs, nodeCount, frameCount, globalDefines, vitals_helper_dir)
    createTelemetry(vitalsNodes, "telemetryDashboard.csv", generated_code_dir, nodeIds, nodeNames, dataNames, vitals_to_telem, globalDefines)
    
    createTelemetryParser(vitals_to_telem, globalEnums)
    createTelemetryRecords(dataPoint_fields, CANFrame_fields)
    
    createPacketSendFiles(generated_code_dir, vitals_helper_dir, vitals_node_dir, nodeNames, nodeIds, vitals_to_telem, telem_to_vitals, globalEnums)

    # --- Generate Callback Skeletons (interactively) ---
    print("\n--- Generating Callback Skeletons ---")
    
    # Java Callbacks
    java_dir = get_telem_path()

    # 1. Generate the fully-generated dispatcher. This file should not be user-edited.
    java_dispatcher_path = os.path.join(java_dir, 'java', 'GeneratedPacketVisitor.java')
    generate_java_visitor_dispatcher(java_dispatcher_path, vitals_to_telem)

    # 2. Generate individual, user-editable skeletons for each packet, checking for existence.
    java_callbacks_dir = os.path.join(java_dir, 'java', 'callbacks')
    os.makedirs(java_callbacks_dir, exist_ok=True)
    for packet in vitals_to_telem:
        skeleton_path = os.path.join(java_callbacks_dir, f"On{packet['name']}Packet.java")
        if not os.path.exists(skeleton_path):
            # This file is a user-editable skeleton, so we only generate it if it doesn't exist.
            generate_single_java_callback_skeleton(skeleton_path, packet)
        # If it already exists, do nothing to preserve user changes.

    # C++ Callbacks
    cpp_callbacks_path = os.path.join(vitals_callbacks_dir, "vitalsRecvCallbacks.cpp")
    interactive_file_gen(cpp_callbacks_path, "C++ Vitals Recv Callbacks", generate_cpp_callbacks, telem_to_vitals)

    # CAN Frame Callbacks (Java)
    print("\n--- Generating CAN Frame Callback Skeletons ---")
    create_can_frame_callbacks(vitalsNodes, nodeNames, nodeIds, dataNames)

    print("Done.")
