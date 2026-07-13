import os
from typing import Any, TextIO

from config.parseFile import Node, CANFrame_fields, ACCESS, expression_to_int
from genUtils import interactive_file_gen
from Lora_Msgs_And_Cmds.genSensorCallbacks import createSensorCommandInfrastructure
from gen_rust_sensor import generate_rust

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
                        base_dir: str) -> None:
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
                    base_dir: str ) -> None:
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
    Unified generator for ALL sensor boards (Arduino, ESP, Rust).
    Generates common C-helper files, then branches for board-specific logic.
    """
    node = node_info.vitals_data
    nodeName = node_info.name
    nodeId = node_info.node_id
    boardType = node_info.board_type
    dataNames = node_info.data_names
    numDataForNode = node_info.num_data

    os.makedirs(sub_dir_path, exist_ok=True)
    
    # 1. Setup Common Helper Directory
    helper_subdir_name = "C_Helper" if boardType == "rust" else SENSOR_HELPER_SUBDIR
    helper_dir = os.path.join(sub_dir_path, helper_subdir_name)
    os.makedirs(helper_dir, exist_ok=True)

    # 2. Generate Common Helper Files (myDefines.hpp & staticDec.cpp)
    with open(os.path.join(helper_dir, 'myDefines.hpp'), 'w') as f:
        _write_my_defines(
            f, nodeName, nodeId, ACCESS(node, 'numFrames')['value'], 
            numDataForNode, has_commands, sensor_commands, dataNames
        )

    sensor_helper_include = "../../../src/sensors/common/sensorHelper.hpp" if boardType == "rust" else "../../common/sensorHelper.hpp"
    with open(os.path.join(helper_dir, 'staticDec.cpp'), 'w') as f:
        _write_static_dec(f, node, sensor_helper_include)

    # 3. Generate Board-Specific Files
    if boardType == "arduino":
        with open(os.path.join(sub_dir_path, 'main.cpp'), 'w') as f:
            _write_arduino_main(f, node, dataNames, base_dir)
            
    elif boardType == "esp":
        with open(os.path.join(sub_dir_path, 'main.c'), 'w') as f:
            _write_esp_main(f, node, dataNames, base_dir)
            
    elif boardType == "rust":
        # Delegate the entire Cargo project and bindgen setup to the dedicated Rust module
        generate_rust(
            nodeName, sub_dir_path, helper_dir, base_dir, 
            node, dataNames, has_commands, sensor_commands
        )
            
    else:
        print(f"Warning: For {nodeName} (node {nodeId}): Please Specify an appropriate board (esp, arduino, rust...)")


def createSensors(nodes: list[Node],
                  base_dir: str,
                  telem_to_vitals: list[dict[str, Any]],
                  c_sensors_dir: str,
                  rust_root_dir: str,
                  generated_platformio_path: str) -> None:

    for node_info in nodes:
        # 1. Determine output directory dynamically based on board type
        if node_info.board_type == "rust":
            sub_dir_path = os.path.join(rust_root_dir, f"{node_info.name}_rust")
        else:
            sub_dir_path = os.path.join(c_sensors_dir, node_info.name)

        # 2. Extract Commands
        sensor_commands = []
        for cmd in telem_to_vitals:
            target = cmd.get("targetNode")
            if not target or target == "vitals": continue

            if target == node_info.name:
                sensor_commands.append(cmd)
                continue
            try:
                if expression_to_int(target) == node_info.node_id:
                    sensor_commands.append(cmd)
            except (ValueError, NameError) as e:
                pass #couldnt match thus cmd onto this specific sensor
                
        has_commands = len(sensor_commands) > 0

        # 3. Single entry point for file generation
        actual_path = interactive_file_gen(
            sub_dir_path, 
            f"Sensor Node '{node_info.name}' ({node_info.board_type})",
            _generate_sensor_files, 
            node_info, base_dir, has_commands, sensor_commands
        )

        # 4. Generate callback infrastructure if needed
        if actual_path and has_commands:
            if node_info.board_type == "rust":
                createSensorCommandInfrastructure(
                    sensor_commands,
                    actual_path,
                    sensor_helper_include="../../../src/sensors/common/sensorHelper.hpp",
                    pecan_include="../../../src/pecan/pecan.h",
                    helper_subdir="C_Helper",
                    generate_callback_stubs=False,
                )
            else:
                createSensorCommandInfrastructure(sensor_commands, actual_path)

    # 5. Generate platformio.ini environments
    with open(generated_platformio_path, 'w') as f:
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