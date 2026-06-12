import os
from config.parseFile import dataPoint_fields, CANFrame_fields, ACCESS, expression_to_int
from Lora_Msgs_And_Cmds.packetFormat import FIXED, CUSTOM
from genUtils import interactive_file_gen
import math
from Lora_Msgs_And_Cmds.packetFormat import telem_to_vitals
from gen_rust_sensor import generate_rust_main

def _write_static_dec(f, node):
    f.write('#include "pecan/pecan.h" // For simpleDataPoint\n'
            '#include "myDefines.hpp"\n#include "../common/sensorHelper.hpp"\n\n'
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

def _write_arduino_main(f, node, dataNames, numDataForNode, base_dir, has_commands):
    main_content = ""
    with open(os.path.join(base_dir, "codeBlocks/sensors/arduinoMain.cpp"), 'r') as fread:
        main_content = fread.read()
    if has_commands:
        main_content = main_content.replace("sensorInit(&plpc, &ts);", "sensorInit(&plpc, &ts);\n\tregisterCommandHandlers(&plpc); // Register command handlers")
    with open(os.path.join(base_dir, "codeBlocks/sensors/arduinoTop.cpp"), 'r') as fread:
        top_content = fread.read()
        if has_commands:
            top_content = top_content.replace('#include "myDefines.hpp"', '#include "myDefines.hpp"\n')
        f.write(top_content)

    current_data_idx = 0
    for frame in ACCESS(node, "CANFrames")["value"]:
        for dataPoint in ACCESS(frame, "dataInfo")["value"]:
            f.write("int32_t collect_{0}(bool* cancelFrameSend){{\n    int32_t {0} = {1};\n"\
                    "\tSerial.println(\"collecting {0}\");\n    return {0};\n}}\n\n".format(
                dataNames[current_data_idx], str(ACCESS(dataPoint, "startingValue")["value"])))
            current_data_idx += 1
    f.write(main_content)

def _write_esp_main(f, node, dataNames, numDataForNode, base_dir, has_commands):
    main_content = ""
    with open(os.path.join(base_dir, "codeBlocks/sensors/espMain.c"), 'r') as fread:
        main_content = fread.read()
    if has_commands:
        main_content = main_content.replace("sensorInit(&plpc, NULL);", "sensorInit(&plpc, NULL);\n\tregisterCommandHandlers(&plpc); // Register command handlers")
    with open(os.path.join(base_dir, "codeBlocks/sensors/espTop.c"), 'r') as fread:
        top_content = fread.read()
        top_content = top_content.replace('#include "../../espBase/debug_esp.h"', '#include "../../espBase/debug_esp.h"\n#include "esp_log.h"\nstatic const char* TAG = "SensorMain";')
        if has_commands:
            top_content = top_content.replace('#include "myDefines.hpp"', '#include "myDefines.hpp"\n')
        f.write(top_content)
    
    current_data_idx = 0
    for frame in ACCESS(node, "CANFrames")["value"]:
        for dataPoint in ACCESS(frame, "dataInfo")["value"]:
            f.write("int32_t collect_{0}(bool* cancelFrameSend){{\n    int32_t {0} = {1};\n"
                    "\tESP_LOGI(TAG, \"collecting {0}\");\n    return {0};\n}}\n\n".format(
                dataNames[current_data_idx], str(ACCESS(dataPoint, "startingValue")["value"])))
            current_data_idx += 1
    f.write(main_content)

def _generate_sensor_files(sub_dir_path, node_info, base_dir, has_commands, sensor_commands, script_dir):
    """
    Helper function to generate all files for a single sensor node.
    This is called by interactive_file_gen.
    """
    node = node_info.vitals_data
    nodeName = node_info.name
    nodeId = node_info.id
    boardType = node_info.board_type
    dataNames = node_info.data_names
    numDataForNode = node_info.num_data

    os.makedirs(sub_dir_path, exist_ok=True)
    # 1. Generate myDefines.hpp
    with open(os.path.join(sub_dir_path, 'myDefines.hpp'), 'w') as f:
        f.write(f'#ifndef {nodeName}_DATA_H\n#define {nodeName}_DATA_H\n')
        f.write(f"//defines constants specific to {nodeName}\n")
        f.write(f'#include <stdint.h>\n#include <stdbool.h>\n#include <stddef.h> // For size_t\n')
        f.write(f"#define myId {nodeId}\n")
        f.write(f"#define numFrames {ACCESS(node, 'numFrames')['value']}\n")
        f.write(f"#define node_numData {numDataForNode}\n\n")
        if has_commands:
            f.write("#define SENSOR_HAS_COMMANDS\n\n")
            for msg in sensor_commands:
                name = msg["name"]
                fields = msg.get("msgFields", [])
                byte_count = msg.get("byteCount")
                struct_name = f"{name}_args_t"
                f.write(f"// ----- {name} -----\n")
                has_struct = len(fields) > 0 or byte_count is CUSTOM
                if has_struct:
                    f.write(f"typedef struct __attribute__((packed)) {{\n")
                    for field in fields: f.write(f"    int32_t {field.name};\n")
                    if byte_count is CUSTOM:
                        f.write("    const uint8_t* payload;\n    size_t max_payload_size;\n")
                    f.write(f"}} {struct_name};\n\n")
                param_str = f"{struct_name} args" if has_struct else "void"
                f.write(f"void on{name}({param_str});\n\n")
        
        for name in dataNames: f.write(f"int32_t collect_{name}(bool* cancelFrameSend);\n")
        
        if has_commands:
            mask_bits = sensor_commands[0].get('can_mask_bits', 0) if sensor_commands else 0
            max_fields = max(len(c.get("msgFields", [])) for c in sensor_commands) if sensor_commands else 0
            f.write(f"\n#define SENSOR_MAX_RECV_DATA_FIELDS {max_fields}\n")
            f.write(f"#define SENSOR_RECV_MASK_BITS {mask_bits}\n")

        f.write("\n#define dataCollectorsList ")
        f.write(', '.join(f"collect_{name}" for name in dataNames))
        f.write("\n\n#endif")

    # 2. Generate main file (main.c / main.cpp / main.rs)
    rust_project_dir = os.path.join(script_dir, "..", "..", f"{nodeName}_rust")
    if boardType == "rust":
        # For Rust, the generated code is a module within the project's src directory
        src_dir = os.path.join(rust_project_dir, "src") # This is <sensor_name>_rust/src/
        os.makedirs(src_dir, exist_ok=True)
        main_path = os.path.join(src_dir, 'sensor_main.rs')
    elif boardType == "arduino":
        main_path = os.path.join(sub_dir_path, 'main.cpp')
    elif boardType == "esp":
        main_path = os.path.join(sub_dir_path, 'main.c')
    else:
        main_path = None

    if main_path:
        with open(main_path, 'w') as f:
            if boardType == "arduino":
                _write_arduino_main(f, node, dataNames, numDataForNode, base_dir, has_commands)
            elif boardType == "esp":
                _write_esp_main(f, node, dataNames, numDataForNode, base_dir, has_commands)
            elif boardType == "rust":
                generate_rust_main(f, node, dataNames, numDataForNode, base_dir, has_commands, sensor_commands) 
    else:
        print(f"Warning: For {nodeName} (node {nodeId}): Please Specify an appropriate board (esp, arduino, rust...)")
        
    # 3. Generate staticDec.cpp
    with open(os.path.join(sub_dir_path, 'staticDec.cpp'), 'w') as f:
        _write_static_dec(f, node)

    # 4. For Rust, generate the wrapper.h for bindgen
    if boardType == "rust":
        wrapper_path = os.path.join(rust_project_dir, 'wrapper.h')
        with open(wrapper_path, 'w') as f:
            f.write("// Wrapper header for bindgen, generated by scripts/genSensors.py\n\n")
            f.write("#include \"../src/programConstants.h\"\n")
            f.write("#include \"../src/sensors/common/sensorHelper.hpp\"\n")

def createSensors(nodes, base_dir, generated_code_dir, telem_to_vitals, globalEnums):

    script_dir = os.path.dirname(os.path.abspath(__file__))
    c_sensors_dir = os.path.join(script_dir, "..", "..", "src", "sensors")
    c_sensors_dir = os.path.normpath(c_sensors_dir)

    common_dir = os.path.join(c_sensors_dir, "common")
    common_dir = os.path.normpath(common_dir)
    os.makedirs(common_dir, exist_ok=True)

    helperPath = os.path.join(common_dir, "sensorHelper.hpp")
    with open(helperPath, 'w') as f:
        f.write("""#include <stdbool.h> // For bool type
#ifndef SENSOR_HELP
#define SENSOR_HELP

#ifdef __cplusplus
extern "C" { //Need C linkage since ESP uses C "C"
#endif
#include "../../programConstants.h"
#include "../../pecan/pecan.h"
#include <stdint.h>
#include <stddef.h> // For size_t

// This needs to be included before its macros are used by other declarations
#define STRINGIZE_(a) #a
#define STRINGIZE(a) STRINGIZE_(a)
#include STRINGIZE(NODE_CONFIG)  //includes node Constants

//universal globals. Used by every sensor
""")
        f.write("typedef struct { //identified by a 2 bit identifier 0-3 in function code\n"
                "    int8_t numData;\n"
                "    int32_t frequency;\n"
                "    int8_t startingDataIndex;  //starting index of data in this frame. used by collector function\n"
                "    simpleDataPoint *dataInfo;\n"
                "} CANFrame;\n\n")

        f.write("extern CANFrame myframes[numFrames];\n\n")
        f.write("//For ts, pass PScheduler* for arduino, else pass NULL\n")
        f.write("int8_t sensorInit(PCANListenParamsCollection* plpc, void* ts);\n")
        f.write("""
#ifdef SENSOR_HAS_COMMANDS
// Struct for command lookup table entries
typedef struct SensorRecvPacketLUTEntry_s {
    const simpleDataPoint* fields;
    uint8_t num_fields;
    bool packetIsCustom;
    void (*callback_wrapper)(const uint8_t* raw_packet, size_t packet_len, int8_t* bitIndex);
} SensorRecvPacketLUTEntry;

void registerCommandHandlers(PCANListenParamsCollection* plpc);

// Extern declarations for the command lookup table, defined in sensorRecvLUT.cpp
extern const SensorRecvPacketLUTEntry sensorRecvPacketLUT[];
extern const size_t sensorRecvPacketLUTSize;
#endif
""")
        f.write("\n#ifdef __cplusplus\n}  // End extern \"C\"\n#endif\n#endif")
    
    # write stuff for each sensor
    for node_info in nodes:
        sensors_dir = c_sensors_dir
        sub_dir_path = os.path.join(sensors_dir, node_info.name)
        # Find commands for this sensor
        sensor_commands = [cmd for cmd in telem_to_vitals if cmd.get("targetNode") == node_info.name]
        has_commands = len(sensor_commands) > 0

        # Use the interactive utility
        actual_path = interactive_file_gen(
            sub_dir_path, 
            f"Sensor Node '{node_info.name}'",
            _generate_sensor_files, # The generation function
            # Args for the generation function:
            node_info, base_dir, has_commands, sensor_commands, script_dir
        )

        if actual_path and has_commands:
            createSensorCommandInfrastructure(node_info.name, node_info.id, sensor_commands, actual_path, globalEnums)

    #Generate platformio.ini environments. Only contains environments for sensor nodes. 
    #Code to be pasted into actual platformio.ini file as an add-on
    file_path = os.path.join(generated_code_dir,'Generatedplatformio.ini')
    with open(file_path, 'w') as f:
        nodeIndex=0
        f.write("\n")
        for node_info in nodes:
            if(node_info.board_type == "arduino"):
                f.write(f"[env:{node_info.name}]\n")
                f.write("extends=arduinoSensorBase\n")
                f.write(f"build_src_filter = ${{arduinoSensorBase.build_src_filter}}"
                        f"+<sensors/{node_info.name}>\n")
                f.write(f"build_flags = -DNODE_CONFIG=../{node_info.name}/myDefines.hpp"
                        "/myDefines.hpp -DSENSOR_ARDUINO_BUILD=ON\n\n")

            elif(node_info.board_type == "esp"):
                f.write(f"[env:{node_info.name}]\n")
                f.write("extends=espSensorBase\n")
                f.write(f"board_build.cmake_extra_args = ${{espSensorBase.board_build.cmake_extra_args}}"
                        f" -DSENS_DIR={node_info.name}\n")
                f.write(f"build_flags = ${{espSensorBase.build_flags}}"
                         f" -DNODE_CONFIG=../{node_info.name}/myDefines.hpp\n\n")
        f.close()

def createSensorCommandInfrastructure(nodeName, nodeId, sensor_commands, sub_dir_path, globalEnums):

    if not sensor_commands: # Should not happen if called correctly
        return # No commands for this sensor

    # 1. Create directory
    helper_dir = os.path.join(sub_dir_path, "commandHelper")
    os.makedirs(helper_dir, exist_ok=True)

    # 2. Assign masks (simple sequential)
    mask_bits = sensor_commands[0].get('can_mask_bits', 0) if sensor_commands else 0
    max_fields = max(len(c.get("msgFields", [])) for c in sensor_commands) if sensor_commands else 0

    # 4. Generate sensorRecvLUT.c
    with open(os.path.join(helper_dir, "sensorRecvLUT.cpp"), 'w') as f:
        f.write('#include "../myDefines.h"\n#include <string.h>\n\nextern "C" {\n\n')
        for msg in sensor_commands:
            name = msg["name"]
            fields = msg.get("msgFields", [])
            if fields:
                f.write(f"const simpleDataPoint {name}_fields[{len(fields)}] = {{\n")
                for field in fields: f.write(f"    {{ {field.min}, {field.max}, {field.bits} }},\n")
                f.write("};\n\n")
        
        for msg in sensor_commands:
            name = msg["name"]
            fields = msg.get("msgFields", [])
            byte_count = msg.get("byteCount")
            struct_name = f"{name}_args_t"
            f.write(f"static void {name}_wrapper(const uint8_t* raw_packet, size_t packet_len, int8_t* bitIndex) {{\n")
            has_struct = len(fields) > 0 or byte_count is CUSTOM
            if has_struct:
                f.write(f"    union {{\n")
                f.write(f"        {struct_name} s;\n")
                if fields:
                    f.write(f"        int32_t data_arr[{len(fields)}];\n")
                f.write(f"    }} u __attribute__((aligned(4)));\n\n")

                if fields:
                    f.write(f"    for (int i = 0; i < {len(fields)}; ++i) {{\n")
                    f.write(f"        pecan_unpack(&u.data_arr[i], raw_packet, &{name}_fields[i], bitIndex);\n")
                    f.write(f"    }}\n")

                if byte_count is CUSTOM:
                    f.write(f"    size_t fixed_bytes = (*bitIndex + 7) / 8;\n")
                    f.write(f"    if (packet_len > fixed_bytes) {{ u.s.payload = raw_packet + fixed_bytes; u.s.max_payload_size = packet_len - fixed_bytes; }}")
                    f.write(f" else {{ u.s.payload = NULL; u.s.max_payload_size = 0; }}\n")

                f.write(f"    on{name}(u.s);\n")
            else:
                f.write(f"    on{name}();\n")
            f.write("}\n\n")

        f.write("const SensorRecvPacketLUTEntry sensorRecvPacketLUT[] = {\n")
        for msg in sensor_commands:
            f.write(f"    {{ // {msg['name']}\n")
            f.write(f"        .fields = {'NULL' if not msg.get('msgFields') else msg['name']+'_fields'},\n")
            f.write(f"        .num_fields = {len(msg.get('msgFields',[]))},\n")
            f.write(f"        .packetIsCustom = {str(msg.get('byteCount') is CUSTOM).lower()},\n")
            f.write(f"        .callback_wrapper = {msg['name']}_wrapper,\n    }},\n")
        f.write("};\n")
        f.write("const size_t sensorRecvPacketLUTSize = sizeof(sensorRecvPacketLUT) / sizeof(SensorRecvPacketLUTEntry);\n")
        f.write("\n} // extern C\n")

    # 5. Generate sensorRecvCallbacks.cpp
    callbacks_path = os.path.join(helper_dir, "sensorRecvCallbacks.cpp")
    if not os.path.exists(callbacks_path):
        with open(callbacks_path, 'w') as f:
            f.write("/**\n")
            f.write(" * @file sensorRecvCallbacks.cpp\n")
            f.write(" * @brief Skeleton implementations for command callbacks.\n")
            f.write(" * NOTE: You may move these implementations to your main.c/cpp file for convenience.\n")
            f.write(" */\n\n")
            f.write('#include "../myDefines.h"\n\n')
            for msg in sensor_commands:
                name, fields, byte_count = msg["name"], msg.get("msgFields", []), msg.get("byteCount")
                struct_name, has_struct = f"{name}_args_t", len(fields) > 0 or byte_count is CUSTOM
                param_str = f"{struct_name} args" if has_struct else "void"
                f.write(f"void on{name}({param_str}) {{\n    // TODO: Implement logic for {name}\n}}\n\n")

# Note: The ACCESS helper is also defined here to allow local field lookup.
def ACCESS(fields, name):
    return next(field for field in fields if field["name"] == name)
