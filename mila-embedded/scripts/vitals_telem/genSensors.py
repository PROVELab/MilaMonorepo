import os
import shutil
from typing import Any, TextIO

from config.parseFile import Node, CANFrame_fields, ACCESS, expression_to_int
from genUtils import interactive_file_gen
from Lora_Msgs_And_Cmds.genSensorCallbacks import createSensorCommandInfrastructure
from gen_rust_sensor import generate_rust_main

SENSOR_HELPER_SUBDIR = "helper"


def _write_my_defines(
    f: TextIO,
    node_name: str,
    node_id: int,
    num_frames: Any,
    num_data_for_node: int,
    has_commands: bool,
    sensor_commands: list[dict[str, Any]],
    data_names: list[str],
) -> None:
    f.write(f'#ifndef {node_name}_DATA_H\n#define {node_name}_DATA_H\n')
    f.write(f"//defines constants specific to {node_name}\n")
    f.write('#include <stdint.h>\n#include <stdbool.h>\n#include <stddef.h> // For size_t\n')
    f.write(f"#define myId {node_id}\n")
    f.write(f"#define numFrames {num_frames}\n")
    f.write(f"#define node_numData {num_data_for_node}\n\n")
    if has_commands:
        f.write("#define SENSOR_HAS_COMMANDS\n\n")

    f.write("#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n")

    if has_commands:
        for msg in sensor_commands:
            name = msg["name"]
            fields = msg.get("msgFields", [])
            contains_payload = msg.get("containsPayload", False)
            struct_name = f"{name}_args_t"
            f.write(f"// ----- {name} -----\n")
            has_struct = len(fields) > 0 or contains_payload
            if has_struct:
                f.write("typedef struct __attribute__((packed)) {\n")
                for field in fields:
                    f.write(f"    int32_t {field.name};\n")
                if contains_payload:
                    f.write("    const uint8_t* payload;\n    size_t max_payload_size;\n")
                f.write(f"}} {struct_name};\n\n")
            param_str = f"{struct_name} args" if has_struct else "void"
            f.write(f"void on{name}({param_str});\n\n")

    for name in data_names:
        f.write(f"int32_t collect_{name}(bool* cancelFrameSend);\n")

    f.write("\n#ifdef __cplusplus\n}\n#endif\n")

    if has_commands:
        mask_bits = sensor_commands[0].get('can_mask_bits', 0) if sensor_commands else 0
        max_fields = max(len(c.get("msgFields", [])) for c in sensor_commands) if sensor_commands else 0
        f.write(f"\n#define SENSOR_MAX_RECV_DATA_FIELDS {max_fields}\n")
        f.write(f"#define SENSOR_RECV_MASK_BITS {mask_bits}\n")

    f.write("\n#define dataCollectorsList ")
    f.write(', '.join(f"collect_{name}" for name in data_names))
    f.write("\n\n#endif\n")

def _generate_rust_sensor_project(
    node_info: Node,
    base_dir: str,
    has_commands: bool,
    sensor_commands: list[dict[str, Any]],
    rust_root_dir: str,
):
    """
    Generates a complete, buildable Cargo project for a Rust-based sensor node.
    """
    nodeName = node_info.name
    nodeId = node_info.node_id
    dataNames = node_info.data_names
    numDataForNode = node_info.num_data
    node = node_info.vitals_data

    # --- 1. Setup project directory and copy common files ---
    cargo_name = nodeName.lower().replace('_', '-')
    rust_project_dir = os.path.join(rust_root_dir, f"{nodeName}_rust")
    template_dir = os.path.join(base_dir, "codeBlocks", "rust")

    if not os.path.isdir(template_dir):
        print(f"Error: Rust common template directory not found at {template_dir}. Cannot generate Rust sensor '{nodeName}'.")
        return

    print(f"Generating Rust sensor project for '{nodeName}' at {os.path.relpath(rust_project_dir)}")
    os.makedirs(rust_project_dir, exist_ok=True)
    c_helper_dir = os.path.join(rust_project_dir, "C_Helper")
    os.makedirs(c_helper_dir, exist_ok=True)

    # Copy common files
    common_files = {
        "build.rs": "build.rs",
        "memory.x": "memory.x",
        "Cargo.toml.template": "Cargo.toml",
        "Embed.toml": "Embed.toml",
        "main.rs.template": os.path.join("src", "main.rs"),
    }

    for src_rel, dest_rel in common_files.items():
        src_path = os.path.join(template_dir, src_rel)
        dest_path = os.path.join(rust_project_dir, dest_rel)

        if not os.path.exists(src_path):
            print(f"Warning: Missing common Rust file: {src_path}")
            continue

        # For main.rs, only copy if it doesn't exist
        if dest_rel == os.path.join("src", "main.rs") and os.path.exists(dest_path):
            continue

        os.makedirs(os.path.dirname(dest_path), exist_ok=True)
        shutil.copy(src_path, dest_path)

    # Customize Cargo.toml
    cargo_toml_path = os.path.join(rust_project_dir, "Cargo.toml")
    if os.path.exists(cargo_toml_path):
        with open(cargo_toml_path, 'r') as f:
            content = f.read()
        content = content.replace("{{PACKAGE_NAME}}", cargo_name)
        with open(cargo_toml_path, 'w') as f:
            f.write(content)

    # --- 2. Generate myDefines.hpp ---
    my_defines_path = os.path.join(c_helper_dir, 'myDefines.hpp')
    with open(my_defines_path, 'w') as f:
        _write_my_defines(
            f,
            nodeName,
            nodeId,
            ACCESS(node, 'numFrames')['value'],
            numDataForNode,
            has_commands,
            sensor_commands,
            dataNames,
        )

    # --- 3. Generate sensor_main.rs ---
    src_dir = os.path.join(rust_project_dir, "src")
    os.makedirs(src_dir, exist_ok=True)
    main_path = os.path.join(src_dir, 'sensor_main.rs')
    interactive_file_gen(
        main_path,
        f"Rust sensor main for '{nodeName}'",
        _generate_rust_sensor_main_file,
        node,
        dataNames,
        numDataForNode,
        base_dir,
        has_commands,
        sensor_commands,
    )

    # --- 4. Generate staticDec.cpp ---
    static_dec_path = os.path.join(c_helper_dir, 'staticDec.cpp')
    with open(static_dec_path, 'w') as f:
        _write_static_dec(f, node, "../../../src/sensors/common/sensorHelper.hpp")

    # --- 5. Generate wrapper.h ---
    wrapper_path = os.path.join(c_helper_dir, 'wrapper.h')
    with open(wrapper_path, 'w') as f:
        f.write("// Wrapper header for bindgen, generated by scripts/genSensors.py\n\n")
        f.write("#include \"../../../src/programConstants.h\"\n")
        f.write("#include \"../../../src/sensors/common/sensorHelper.hpp\"\n")

    # --- 6. Generate command infrastructure if needed ---
    if has_commands:
        createSensorCommandInfrastructure(
            sensor_commands,
            rust_project_dir,
            sensor_helper_include="../../../src/sensors/common/sensorHelper.hpp",
            pecan_include="../../../src/pecan/pecan.h",
            helper_subdir="C_Helper",
            generate_callback_stubs=False,
        )

def _generate_rust_sensor_main_file(
    output_path: str,
    node: list[dict[str, Any]],
    dataNames: list[str],
    numDataForNode: int,
    base_dir: str,
    has_commands: bool,
    sensor_commands: list[dict[str, Any]],
) -> None:
    with open(output_path, 'w') as f:
        generate_rust_main(f, node, dataNames, numDataForNode, base_dir, has_commands, sensor_commands)

def _write_static_dec(
    f: TextIO,
    node: list[dict[str, Any]],
    sensor_helper_include: str = "../common/sensorHelper.hpp",
) -> None:
    f.write('#include "pecan/pecan.h" // For simpleDataPoint\n'
            f'#include "myDefines.hpp"\n#include "{sensor_helper_include}"\n\n'
            '#ifdef __cplusplus\n'
            'extern "C" {\n'
            '#endif\n'
            '//creates CANFrame array from this node. It stores data to be sent, and info for how to send\n\n')
    
    frameNum = 0
    for frame in ACCESS(node, "CANFrames")["value"]:
        num_Data = ACCESS(frame, "numData")["value"]
        f.write(f"simpleDataPoint f{frameNum}DataPoints [{num_Data}]={{\n")
        
        for dataPoint in ACCESS(frame, "dataInfo")["value"]:
            min_val = expression_to_int(ACCESS(dataPoint, "min")["value"])
            max_val = expression_to_int(ACCESS(dataPoint, "max")["value"])
            bits_val = expression_to_int(ACCESS(dataPoint, "bits")["value"])
            f.write(f"    {{ .min={min_val}, .max={max_val}, .bits={bits_val} }},\n")
        f.write("};\n\n")
        frameNum += 1

    frame_index = 0
    localDataIndex = 0
    f.write("CANFrame myframes[numFrames] = {\n")
    for frame in ACCESS(node, "CANFrames")["value"]:
        f.write("    {")
        first = True
        for field in CANFrame_fields:
            if "sensor" in field["node"]:
                if not first:
                    f.write(", ")
                if field['type'] == "boolean":
                    value = "true" if expression_to_int(ACCESS(frame, field['name'])['value']) == 1 else "false"
                    f.write(f".{field['name']} = {value}")
                else:
                    f.write(f".{field['name']} = {ACCESS(frame, field['name'])['value']}")
                first = False
        f.write(f", .startingDataIndex={localDataIndex}")
        localDataIndex += ACCESS(frame, "numData")["value"]
        f.write(f", .dataInfo=f{frame_index}DataPoints")
        f.write("},\n")
        frame_index += 1
    f.write("};\n")
    f.write("\n#ifdef __cplusplus\n}\n#endif\n")

def _write_arduino_main(f: TextIO,
                        node: list[dict[str, Any]],
                        dataNames: list[str],
                        numDataForNode: int,
                        base_dir: str,
                        has_commands: bool) -> None:
    with open(os.path.join(base_dir, "codeBlocks/c_sensors/arduinoMain.cpp"), 'r') as fread:
        main_content = fread.read()
    with open(os.path.join(base_dir, "codeBlocks/c_sensors/arduinoTop.cpp"), 'r') as fread:
        top_content = fread.read()
        f.write(top_content)

    current_data_idx = 0
    for frame in ACCESS(node, "CANFrames")["value"]:
        for dataPoint in ACCESS(frame, "dataInfo")["value"]:
            f.write("int32_t collect_{0}(bool* cancelFrameSend){{\n    int32_t {0} = {1};\n"\
                    "\tSerial.println(\"collecting {0}\");\n    return {0};\n}}\n\n".format(
                dataNames[current_data_idx], str(ACCESS(dataPoint, "startingValue")["value"])))
            current_data_idx += 1
    f.write(main_content)

def _write_esp_main(f: TextIO,
                    node: list[dict[str,
                                    Any]],
                    dataNames: list[str],
                    numDataForNode: int,
                    base_dir: str,
                    has_commands: bool) -> None:
    with open(os.path.join(base_dir, "codeBlocks/c_sensors/espMain.c"), 'r') as fread:
        main_content = fread.read()
    with open(os.path.join(base_dir, "codeBlocks/c_sensors/espTop.c"), 'r') as fread:
        top_content = fread.read()
        f.write(top_content)
    
    current_data_idx = 0
    for frame in ACCESS(node, "CANFrames")["value"]:
        for dataPoint in ACCESS(frame, "dataInfo")["value"]:
            f.write("int32_t collect_{0}(bool* cancelFrameSend){{\n    int32_t {0} = {1};\n"
                    "\tESP_LOGI(TAG, \"collecting {0}\");\n    return {0};\n}}\n\n".format(
                dataNames[current_data_idx], str(ACCESS(dataPoint, "startingValue")["value"])))
            current_data_idx += 1
    f.write(main_content)

def _generate_sensor_files(sub_dir_path: str,
                           node_info: Node,
                           base_dir: str,
                           has_commands: bool,
                           sensor_commands: list[dict[str, Any]]) -> None:
    """
    Helper function to generate all files for a single sensor node.
    This is called by interactive_file_gen.
    """
    node = node_info.vitals_data
    nodeName = node_info.name
    nodeId = node_info.node_id
    boardType = node_info.board_type
    dataNames = node_info.data_names
    numDataForNode = node_info.num_data

    os.makedirs(sub_dir_path, exist_ok=True)
    helper_dir = os.path.join(sub_dir_path, SENSOR_HELPER_SUBDIR)
    os.makedirs(helper_dir, exist_ok=True)
    # 1. Generate myDefines.hpp
    with open(os.path.join(helper_dir, 'myDefines.hpp'), 'w') as f:
        _write_my_defines(
            f,
            nodeName,
            nodeId,
            ACCESS(node, 'numFrames')['value'],
            numDataForNode,
            has_commands,
            sensor_commands,
            dataNames,
        )

    # 2. Generate main file (main.c / main.cpp / main.rs)
    if boardType == "arduino":
        main_path = os.path.join(sub_dir_path, 'main.cpp')
    elif boardType == "esp":
        main_path = os.path.join(sub_dir_path, 'main.c')
    else:
        # Rust projects are handled separately in createSensors
        main_path = None

    if main_path:
        with open(main_path, 'w') as f:
            if boardType == "arduino":
                _write_arduino_main(f, node, dataNames, numDataForNode, base_dir, has_commands)
            elif boardType == "esp":
                _write_esp_main(f, node, dataNames, numDataForNode, base_dir, has_commands)
    else:
        if boardType not in ["arduino", "esp", "rust"]:
            print(f"Warning: For {nodeName} (node {nodeId}): Please Specify an appropriate board (esp, arduino, rust...)")
        
    # 3. Generate staticDec.cpp
    with open(os.path.join(helper_dir, 'staticDec.cpp'), 'w') as f:
        _write_static_dec(f, node, "../../common/sensorHelper.hpp")

def createSensors(nodes: list[Node],
                  base_dir: str,
                  telem_to_vitals: list[dict[str, Any]],
                  c_sensors_dir: str,
                  rust_root_dir: str,
                  generated_platformio_path: str) -> None:

    # write content in directories of each sensor node
    for node_info in nodes:
        sensors_dir = c_sensors_dir
        sub_dir_path = os.path.join(sensors_dir, node_info.name)
        # Find commands for this sensor
        sensor_commands = []
        for cmd in telem_to_vitals:
            target = cmd.get("targetNode")
            if not target:
                continue

            # 1. Match by direct name comparison
            if target == node_info.name:
                sensor_commands.append(cmd)
                continue

            # 2. Match by evaluating target as an ID expression
            try:
                target_id = expression_to_int(target)
                if target_id == node_info.node_id:
                    sensor_commands.append(cmd)
            except (ValueError, NameError):
                # This is expected if 'target' is a name that doesn't match,
                # or not a valid expression.
                pass
        has_commands = len(sensor_commands) > 0

        if node_info.board_type == "rust":
            # For Rust, we generate a self-contained Cargo project.
            # We don't use interactive_file_gen as it's designed for single files.
            print(f"\n--- Generating Rust Sensor Node '{node_info.name}' ---")
            _generate_rust_sensor_project(
                node_info, base_dir, has_commands, sensor_commands, rust_root_dir
            )
            continue # Move to the next node

        actual_path = interactive_file_gen(
            sub_dir_path, 
            f"Sensor Node '{node_info.name}'",
            _generate_sensor_files, # The generation function
            # Args for the generation function:
            node_info, base_dir, has_commands, sensor_commands
        )

        if actual_path and has_commands:
            createSensorCommandInfrastructure(sensor_commands, actual_path)

    #Generate platformio.ini environments. Only contains environments for sensor nodes. 
    #Code to be pasted into actual platformio.ini file as an add-on
    with open(generated_platformio_path, 'w') as f:
        nodeIndex=0
        f.write("\n")
        for node_info in nodes:
            if(node_info.board_type == "arduino"):
                f.write(f"[env:{node_info.name}]\n")
                f.write("extends=arduinoSensorBase\n")
                f.write(f"build_src_filter = ${{arduinoSensorBase.build_src_filter}}"
                        f"+<sensors/{node_info.name}>\n")
                f.write(
                    f"build_flags = -DNODE_CONFIG=../{node_info.name}/{SENSOR_HELPER_SUBDIR}/myDefines.hpp "
                    "-DSENSOR_ARDUINO_BUILD=ON\n\n"
                )

            elif(node_info.board_type == "esp"):
                f.write(f"[env:{node_info.name}]\n")
                f.write("extends=espSensorBase\n")
                f.write(f"board_build.cmake_extra_args = ${{espSensorBase.board_build.cmake_extra_args}}"
                        f" -DSENS_DIR={node_info.name}\n")
                f.write(f"build_flags = ${{espSensorBase.build_flags}}"
                         f" -DNODE_CONFIG=../{node_info.name}/{SENSOR_HELPER_SUBDIR}/myDefines.hpp\n\n")
        f.close()
