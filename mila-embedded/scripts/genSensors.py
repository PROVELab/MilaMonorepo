import os
from parseFile import dataPoint_fields, CANFrame_fields, ACCESS, expression_to_int
from packetFormat import FIXED, CUSTOM
from genUtils import interactive_file_gen
import math
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
    startingDataIndex = 0
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
        f.write(f", .startingDataIndex={startingDataIndex}")
        startingDataIndex += ACCESS(frame, "numData")["value"]
        f.write(f", .dataInfo=f{frame_index}DataPoints")
        f.write("},\n")
        frame_index += 1
    f.write("};\n")
    f.write("\n#ifdef __cplusplus\n}\n#endif\n")

def _write_arduino_main(f, node, dataNames, numDataForNode, localDataIndex, base_dir, has_commands):
    main_content = ""
    with open(os.path.join(base_dir, "codeBlocks/sensors/arduinoMain.cpp"), 'r') as fread:
        main_content = fread.read()
    if has_commands:
        main_content = main_content.replace("sensorInit(&plpc, &ts);", "sensorInit(&plpc, &ts);\n\tregisterCommandHandlers(&plpc);")

    with open(os.path.join(base_dir, "codeBlocks/sensors/arduinoTop.cpp"), 'r') as fread:
        top_content = fread.read()
        if has_commands:
            top_content = top_content.replace('#include "myDefines.hpp"', '#include "myDefines.hpp"\n#ifdef SENSOR_HAS_COMMANDS\n#include "commandHelper/command_handler.h"\n#endif')
        f.write(top_content)

    current_data_idx = localDataIndex
    for frame in ACCESS(node, "CANFrames")["value"]:
        for dataPoint in ACCESS(frame, "dataInfo")["value"]:
            f.write("int32_t collect_{0}(bool* cancelFrameSend){{\n    int32_t {0} = {1};\n"\
                    "\tSerial.println(\"collecting {0}\");\n    return {0};\n}}\n\n".format(
                dataNames[current_data_idx], str(ACCESS(dataPoint, "startingValue")["value"])))
            current_data_idx += 1
    f.write(main_content)

def _write_esp_main(f, node, dataNames, numDataForNode, localDataIndex, base_dir, has_commands):
    main_content = ""
    with open(os.path.join(base_dir, "codeBlocks/sensors/espMain.c"), 'r') as fread:
        main_content = fread.read()
    if has_commands:
        main_content = main_content.replace("sensorInit(&plpc, NULL);", "sensorInit(&plpc, NULL);\n\tregisterCommandHandlers(&plpc);")

    with open(os.path.join(base_dir, "codeBlocks/sensors/espTop.c"), 'r') as fread:
        top_content = fread.read()
        top_content = top_content.replace('#include "../../espBase/debug_esp.h"', '#include "../../espBase/debug_esp.h"\n#include "esp_log.h"\nstatic const char* TAG = "SensorMain";')
        if has_commands:
            top_content = top_content.replace('#include "myDefines.hpp"', '#include "myDefines.hpp"\n#ifdef SENSOR_HAS_COMMANDS\n#include "commandHelper/command_handler.h"\n#endif')
        f.write(top_content)
    
    current_data_idx = localDataIndex
    for frame in ACCESS(node, "CANFrames")["value"]:
        for dataPoint in ACCESS(frame, "dataInfo")["value"]:
            f.write("int32_t collect_{0}(bool* cancelFrameSend){{\n    int32_t {0} = {1};\n"
                    "\tESP_LOGI(TAG, \"collecting {0}\");\n    return {0};\n}}\n\n".format(
                dataNames[current_data_idx], str(ACCESS(dataPoint, "startingValue")["value"])))
            current_data_idx += 1
    f.write(main_content)

def _generate_sensor_files(sub_dir_path, node, nodeName, nodeId, boardType, dataNames, numDataForNode, localDataIndex, base_dir, has_commands):
    """
    Helper function to generate all files for a single sensor node.
    This is called by interactive_file_gen.
    """
    os.makedirs(sub_dir_path, exist_ok=True)
    # 1. Generate myDefines.hpp
    with open(os.path.join(sub_dir_path, 'myDefines.hpp'), 'w') as f:
        f.write(f'#ifndef {nodeName}_DATA_H\n#define {nodeName}_DATA_H\n')
        f.write(f"//defines constants specific to {nodeName}\n")
        f.write(f'#include <stdint.h>\n#include <stdbool.h>\n')
        f.write(f"#define myId {nodeId}\n")
        f.write(f"#define numFrames {ACCESS(node, 'numFrames')['value']}\n")
        f.write(f"#define node_numData {numDataForNode}\n\n")
        if has_commands:
            f.write("#define SENSOR_HAS_COMMANDS\n\n")
        
        for i in range(numDataForNode):
            f.write(f"int32_t collect_{dataNames[localDataIndex + i]}(bool* cancelFrameSend);\n")
        
        f.write("\n#define dataCollectorsList ")
        f.write(', '.join(f"collect_{name}" for name in dataNames[localDataIndex : localDataIndex + numDataForNode]))
        f.write("\n\n#endif")

    # 2. Generate main file (main.c / main.cpp / main.rs)
    if boardType == "rust":
        # Route Rust source code to ../<sensor_name>/src
        src_dir = os.path.join("..", nodeName, "src")
        os.makedirs(src_dir, exist_ok=True)
        main_path = os.path.join(src_dir, 'main.rs')
    elif boardType == "arduino":
        main_path = os.path.join(sub_dir_path, 'main.cpp')
    elif boardType == "esp":
        main_path = os.path.join(sub_dir_path, 'main.c')
    else:
        main_path = None

    if main_path:
        with open(main_path, 'w') as f:
            if boardType == "arduino":
                _write_arduino_main(f, node, dataNames, numDataForNode, localDataIndex, base_dir, has_commands)
            elif boardType == "esp":
                _write_esp_main(f, node, dataNames, numDataForNode, localDataIndex, base_dir, has_commands)
            elif boardType == "rust":
                generate_rust_main(f, node, dataNames, numDataForNode, localDataIndex, base_dir, has_commands) 
    else:
        print(f"Warning: For {nodeName} (node {nodeId}): Please Specify an appropriate board (esp, arduino, rust...)")
        
    # 3. Generate staticDec.cpp
    with open(os.path.join(sub_dir_path, 'staticDec.cpp'), 'w') as f:
        _write_static_dec(f, node)

def createSensors(nodes, base_dir, generated_code_dir, telem_to_vitals, globalEnums):

    all_data_names = [name for node in nodes for name in node.data_names]

    script_dir = os.path.dirname(os.path.abspath(__file__))
    c_sensors_dir = os.path.join(script_dir, "..", "src", "sensors")
    c_sensors_dir = os.path.normpath(c_sensors_dir)

    common_dir = os.path.join(c_sensors_dir, "common")
    common_dir = os.path.normpath(common_dir)
    os.makedirs(common_dir, exist_ok=True)

    helperPath = os.path.join(common_dir, "sensorHelper.hpp")
    with open(helperPath, 'w') as f:
        f.write("#include <stdbool.h> // For bool type\n") # ADDED
        with open(os.path.join(base_dir, "codeBlocks/sensors/helpTop.c"), 'r') as fread: # This file likely contains other includes or boilerplate
            f.write(fread.read())
            fread.close()

        # The dataPoint struct is now simpleDataPoint in pecan.h, so we don't define it here.
        # write the CANFrame struct
        f.write("typedef struct {    //identified by a 2 bit identifier 0-3 in function code\n")
        for field in CANFrame_fields:
            if "sensor" in field["node"]:
                # f.write("    " + field["type"] + " " + field["name"] + ";\n")     
        # custom fields here
                c_type = "bool" if field['type'] == "boolean" else field['type']
                f.write(f"    {c_type} {field['name']};\n")
        f.write("    int8_t startingDataIndex;  //starting index of data in this frame. used by collector function\n")
        f.write("    simpleDataPoint *dataInfo;\n")
        f.write("} CANFrame;\n")
        #

        f.write("extern CANFrame myframes[numFrames];    //defined in myDefines.hpp in <sensor_name> folder\n\n"
        "//shortened versions of vitals structs, containing only stuff the sensors need for sending\n")

        f.write("//For ts, pass PScheduler* for arduino, else pass NULL\n")
        f.write("int8_t sensorInit(PCANListenParamsCollection* plpc, void* ts);\n")
        f.write("#ifdef __cplusplus\n}  // End extern \"C\"\n#endif\n#endif")
    
    # write stuff for each sensor
    nodeIndex = 0
    dataIndex = 0
    for node_info in nodes:
        if node_info.board_type == "rust":
            sensors_dir = os.path.join(script_dir, "..", node_info.name + "_sensor", "c_src")
            sensors_dir = os.path.normpath(sensors_dir)
            os.makedirs(sensors_dir, exist_ok=True)
        else:
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
            node_info.vitals_data, node_info.name, node_info.id, node_info.board_type, all_data_names, node_info.num_data, dataIndex, base_dir, has_commands
        )

        if actual_path:
            createSensorCommandInfrastructure(node_info.name, node_info.id, telem_to_vitals, actual_path, globalEnums)
        
        dataIndex += node_info.num_data

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

def createSensorCommandInfrastructure(nodeName, nodeId, commands, sub_dir_path, globalEnums):
    # Filter commands for this sensor
    sensor_commands = [cmd for cmd in commands if cmd.get("targetNode") == nodeName]
    
    if not sensor_commands:
        return # No commands for this sensor

    # 1. Create directory
    helper_dir = os.path.join(sub_dir_path, "commandHelper")
    os.makedirs(helper_dir, exist_ok=True)

    # 2. Assign masks (simple sequential)
    mask_bits = math.ceil(math.log2(len(sensor_commands))) if sensor_commands else 0
    for i, cmd in enumerate(sensor_commands):
        cmd['mask'] = i
        cmd['mask_bits'] = mask_bits

    max_fields = max(len(c.get("msgFields", [])) for c in sensor_commands) if sensor_commands else 0

    # 3. Generate sensorRecvLUT.h
    with open(os.path.join(helper_dir, "sensorRecvLUT.h"), 'w') as f:
        f.write("#ifndef SENSOR_RECV_LUT_H\n#define SENSOR_RECV_LUT_H\n\n")
        f.write("#ifdef __cplusplus\n")
        f.write("extern \"C\" {\n")
        f.write("#endif\n\n")
        f.write('#include "pecan/pecan.h"\n#include <stddef.h>\n#include <stdint.h>\n\n')
        f.write(f"#define SENSOR_MAX_RECV_DATA_FIELDS {max_fields}\n") # This is for the number of fields in the struct, not the max mask bits
        f.write("#define SENSOR_RECV_PACKET_TYPE_FIXED 0\n#define SENSOR_RECV_PACKET_TYPE_CUSTOM 1\n\n")

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

        f.write("typedef struct SensorRecvPacketLUTEntry {\n")
        f.write("    const simpleDataPoint* fields;\n    uint8_t num_fields;\n")
        f.write("    uint8_t packet_type;\n")
        f.write("    void (*callback_wrapper)(const uint8_t* raw_packet, size_t packet_len, int8_t* bitIndex);\n")
        f.write("} SensorRecvPacketLUTEntry;\n\n")
        f.write(f"extern const int SENSOR_RECV_MASK_BITS;\n")
        f.write(f"extern const SensorRecvPacketLUTEntry sensorRecvPacketLUT[];\n")
        f.write(f"extern const size_t sensorRecvPacketLUTSize;\n")
        f.write("\n#ifdef __cplusplus\n")
        f.write("}\n")
        f.write("#endif\n\n#endif // SENSOR_RECV_LUT_H\n")

    # 4. Generate sensorRecvLUT.c
    with open(os.path.join(helper_dir, "sensorRecvLUT.cpp"), 'w') as f:
        f.write('#include "sensorRecvLUT.h"\n#include <string.h>\n\nextern "C" {\n\n')
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

        f.write(f"const int SENSOR_RECV_MASK_BITS = {mask_bits};\n")
        f.write("const SensorRecvPacketLUTEntry sensorRecvPacketLUT[] = {\n")
        for msg in sensor_commands:
            f.write(f"    {{ // {msg['name']}\n")
            f.write(f"        /* .fields = */ {'NULL' if not msg.get('msgFields') else msg['name']+'_fields'},\n")
            f.write(f"        /* .num_fields = */ {len(msg.get('msgFields',[]))},\n")
            f.write(f"        /* .packet_type = */ SENSOR_RECV_PACKET_TYPE_{'CUSTOM' if msg.get('byteCount') is CUSTOM else 'FIXED'},\n")
            f.write(f"        /* .callback_wrapper = */ {msg['name']}_wrapper,\n    }},\n")
        f.write("};\n")
        f.write("const size_t sensorRecvPacketLUTSize = sizeof(sensorRecvPacketLUT) / sizeof(SensorRecvPacketLUTEntry);\n")
        f.write("\n} // extern C\n")

    # 5. Generate sensorRecvCallbacks.cpp
    with open(os.path.join(helper_dir, "sensorRecvCallbacks.cpp"), 'w') as f:
        f.write('#include "sensorRecvLUT.h"\n\n')
        for msg in sensor_commands:
            name = msg["name"]
            fields = msg.get("msgFields", [])
            byte_count = msg.get("byteCount")
            struct_name = f"{name}_args_t"
            has_struct = len(fields) > 0 or byte_count is CUSTOM
            param_str = f"{struct_name} args" if has_struct else "void"
            f.write(f"void on{name}({param_str}) {{\n    // TODO: Implement logic for {name}\n}}\n\n")

    # 6. Generate command_handler.h
    with open(os.path.join(helper_dir, "command_handler.h"), 'w') as f:
        f.write("#ifndef COMMAND_HANDLER_H\n#define COMMAND_HANDLER_H\n\n")
        f.write('#include "pecan/pecan.h"\n\n')
        f.write("#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n")
        f.write("void registerCommandHandlers(PCANListenParamsCollection* plpc);\n\n")
        f.write("#ifdef __cplusplus\n}\n#endif\n\n#endif\n")

    # 7. Generate command_handler.c
    with open(os.path.join(helper_dir, "command_handler.cpp"), 'w') as f:
        f.write('#include "command_handler.h"\n')
        f.write('#include "sensorRecvLUT.h"\n')
        f.write('#include "myDefines.hpp"\n')
        f.write('#include <string.h>\n\n')
        f.write("""extern "C" {

static int16_t handleTelemetryCommand(CANPacket* p) {
    const uint8_t* data = p->data;
    size_t len = p->dataSize;
    
    if (len == 0) return -1;

    int8_t bitIndex = 0;
    int32_t mask_val = 0;

    if (SENSOR_RECV_MASK_BITS > 0) {
        simpleDataPoint mask_field;
        mask_field.min = 0;
        mask_field.max = 0;
        mask_field.bits = SENSOR_RECV_MASK_BITS;
        pecan_unpack(&mask_val, data, &mask_field, &bitIndex);
    }

    if (mask_val >= sensorRecvPacketLUTSize) {
        return -1; // Invalid mask
    }

    const SensorRecvPacketLUTEntry* entry = &sensorRecvPacketLUT[mask_val];
    if (entry->callback_wrapper) {
        entry->callback_wrapper(data, len, &bitIndex);
    }
    return 0;
}

void registerCommandHandlers(PCANListenParamsCollection* plpc) {
    CANListenParam telemCommandParam;
    telemCommandParam.listen_id = combinedID(TelemetryCommand, myId);
    telemCommandParam.handler = handleTelemetryCommand;
    telemCommandParam.mt = MATCH_EXACT;
    if (addParam(plpc, telemCommandParam) != SUCCESS) {
        // TODO: Handle error, maybe with a print
    }
}

} // extern C
""")

# Note: The ACCESS helper is also defined here to allow local field lookup.
def ACCESS(fields, name):
    return next(field for field in fields if field["name"] == name)
