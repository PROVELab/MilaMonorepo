import os
from parseFile import dataPoint_fields, CANFrame_fields, ACCESS
from packetFormat import FIXED, CUSTOM
import math

def createSensors(vitalsNodes, nodeNames, boardTypes, nodeIds, dataNames, numData, base_dir, generated_code_dir, telem_to_vitals, globalEnums):

    script_dir = os.path.dirname(os.path.abspath(__file__))
    sensors_dir = os.path.join(script_dir, "..", "src", "sensors")
    sensors_dir = os.path.normpath(sensors_dir)

    common_dir = os.path.join(sensors_dir, "common")
    common_dir = os.path.normpath(common_dir)
    os.makedirs(common_dir, exist_ok=True)    

    helperPath = os.path.join(common_dir, "sensorHelper.hpp")
    with open(helperPath, 'w') as f:
        with open(os.path.join(base_dir, "codeBlocks/sensors/helpTop.c"), 'r') as fread:
            f.write(fread.read())
            fread.close()

        # The dataPoint struct is now simpleDataPoint in pecan.h, so we don't define it here.
        
        # write the CANFrame struct
        f.write("typedef struct{    //identified by a 2 bit identifier 0-3 in function code\n")
        for field in CANFrame_fields:
            if "sensor" in field["node"]:
                f.write("    " + field["type"] + " " + field["name"] + ";\n")     
        # custom fields here
        f.write("    int8_t startingDataIndex;  //starting index of data in this frame. used by collector function\n")
        f.write("    simpleDataPoint *dataInfo;\n")
        f.write("} CANFrame;\n")
        #

        f.write("extern CANFrame myframes[numFrames];    //defined in sensorStaticDec.cpp in <sensor_name> folder\n\n"
        "//shortened versions of vitals structs, containing only stuff the sensors need for sending\n")

        f.write("//For ts, pass PScheduler* for arduino, else pass NULL\n")
        f.write("int8_t sensorInit(PCANListenParamsCollection* plpc, void* ts);\n")
        f.write("#ifdef __cplusplus\n}  // End extern \"C\"\n#endif\n#endif")
    
    # write stuff for each sensor
    nodeIndex = 0
    dataIndex = 0
    copyAll = False
    while nodeIndex < len(vitalsNodes):
        node = vitalsNodes[nodeIndex]
        # For sensors, do not overwrite, just create a copy with a unique name. They user can easily drag over as needed. This avoids overwriting user edited sensor main files        # Define the base name
        sub_dir_path = os.path.join(sensors_dir, nodeNames[nodeIndex])
        # If the directory exists, find a unique "CopyX" name
        if os.path.exists(sub_dir_path):
            if (copyAll):
                counter = 1
                # Keep incrementing until the path does not exist
                while os.path.exists(f"{sub_dir_path}_Copy{counter}"):
                    counter += 1
                
                # Update the path to the unique Copy version
                sub_dir_path = f"{sub_dir_path}_Copy{counter}"
            else:
                print(f"\nDirectory {os.path.relpath(sub_dir_path)} already exists. Press 'h' or 'help' for help")
                response = input("Do you want to make copy, overwrite, skip, copy all, or skip all (c/o/s/ca/sa)? : ").strip().lower()
                if response == 'o' or response == 'overwrite':
                    # User chose to overwrite, so we can remove the existing directory
                    import shutil
                    counter = 1 #remove all existing copies for this sensor
                    while(os.path.exists(sub_dir_path)):
                        shutil.rmtree(sub_dir_path)
                        sub_dir_path= os.path.join(sensors_dir, nodeNames[nodeIndex])
                        sub_dir_path = f"{sub_dir_path}_Copy{counter}"
                        counter += 1
                    sub_dir_path = os.path.join(sensors_dir, nodeNames[nodeIndex])
                elif response == 's' or response == 'skip':
                    print(f"Skipping generation for {nodeNames[nodeIndex]}")
                    nodeIndex += 1
                    dataIndex += numData[nodeIndex - 1]
                    continue
                elif response == 'sa' or response == 'skip all':
                    print(f"Skipping all remaining sensor generations.")
                    break
                elif response == 'c' or response == 'copy' or response == 'ca' or response == 'copy all':
                    counter = 1
                    # Keep incrementing until the path does not exist
                    while os.path.exists(f"{sub_dir_path}_Copy{counter}"):
                        counter += 1
                    
                    # Update the path to the unique Copy version
                    sub_dir_path = f"{sub_dir_path}_Copy{counter}"
                elif response == 'h' or response == 'help':
                    print("\"Copying\" a sensor node means making a new folder with a unique name, \n" \
                      "like <name>CopyX. Do this if you are updating sensor node parameters, \n" \
                      "but have custom user code for that sensor you want to save and copy over\n" \
                      "Overwrite will delete all existing folders for that sensor, as well as any manual user code,\n" \
                      "Only do this if you do not care about existing user code for that sensor node.\n")
                    continue
                else: 
                    print("invalid response, try again")
                    continue
                if(response == 'ca' or response == 'copy all'):
                    copyAll = True  # do not ask again

        # Create the directory (exist_ok=True is safe but technically redundant now)
        os.makedirs(sub_dir_path, exist_ok=True)

        # Proceed with writing your file
        file_path = os.path.join(sub_dir_path, 'myDefines.hpp')
        with open(file_path, 'w') as f:
            # Find commands for this sensor
            sensor_commands = [cmd for cmd in telem_to_vitals if cmd.get("targetNode") == nodeNames[nodeIndex]]
            has_commands = len(sensor_commands) > 0

            # includes
            f.write('#ifndef ' + nodeNames[nodeIndex] + '_DATA_H\n#define ' + nodeNames[nodeIndex] + '_DATA_H\n')
            f.write("//defines constants specific to " + nodeNames[nodeIndex])
            f.write('#include "../common/sensorHelper.hpp"\n#include<stdint.h>\n')
            f.write("#define myId " + str(nodeIds[nodeIndex]))
            f.write("\n#define numFrames " + str(ACCESS(node, "numFrames")["value"]))
            f.write("\n#define node_numData " + str(numData[nodeIndex]) + "\n\n")
            f.write("\n#define node_numData " + str(numData[nodeIndex]) + "\n")
            if has_commands:
                f.write("#define SENSOR_HAS_COMMANDS\n\n")
            localDataIndex = dataIndex
            for i in range(numData[nodeIndex]):
                f.write("int32_t collect_" + dataNames[dataIndex] + "(bool* cancelFrameSend);\n")
                localDataIndex += 1
                dataIndex += 1  # increment dataIndex for each function declared
            f.write("\n#define dataCollectorsList ")
            f.write(', '.join("collect_" + name\
                    for name in dataNames[localDataIndex - numData[nodeIndex]: localDataIndex]))
            f.write("\n\n#endif")
        file_path
        if(boardTypes[nodeIndex]=="arduino"):
            file_path = os.path.join(sub_dir_path, 'main.cpp')
        elif(boardTypes[nodeIndex]=="esp"):
            file_path = os.path.join(sub_dir_path, 'main.c')

        else:
            print(f"Warning: For {nodeNames[nodeIndex]} (node {nodeIds[nodeIndex]})\
                  : Please Specify an appropraite board (esp, arduino, ...?)")
            while(1): pass

        # Generate command infrastructure if needed
        createSensorCommandInfrastructure(nodeNames[nodeIndex], nodeIds[nodeIndex], telem_to_vitals, sub_dir_path, globalEnums)

        with open(file_path, 'w') as f:
            if(boardTypes[nodeIndex]=="arduino"):    #create main.cpp for arduino sensors
                # Inject command handler registration if needed
                main_content = ""
                with open(os.path.join(base_dir, "codeBlocks/sensors/arduinoMain.cpp"), 'r') as fread:
                    main_content = fread.read()
                if has_commands:
                    main_content = main_content.replace("sensorInit(&plpc, &ts);", "sensorInit(&plpc, &ts);\n\tregisterCommandHandlers(&plpc);")

                with open(os.path.join(base_dir, "codeBlocks/sensors/arduinoTop.cpp"), 'r') as fread:
                    f.write(fread.read())
                    f.write(fread.read().replace('#include "myDefines.hpp"', '#include "myDefines.hpp"\n#ifdef SENSOR_HAS_COMMANDS\n#include "commandHelper/command_handler.h"\n#endif'))
                    fread.close()

                localDataIndex = dataIndex - numData[nodeIndex]  # reset localDataIndex for this node
                for frame in ACCESS(node, "CANFrames")["value"]:
                    for dataPoint in ACCESS(frame, "dataInfo")["value"]:
                        f.write("int32_t collect_{0}(bool* cancelFrameSend){{\n    int32_t {0} = {1};\n"\
                                "\tSerial.println(\"collecting {0}\");\n    return {0};\n}}\n\n".format(
                            dataNames[localDataIndex], str(ACCESS(dataPoint, "startingValue")["value"])))
                        localDataIndex += 1
                with open(os.path.join(base_dir, "codeBlocks/sensors/arduinoMain.cpp"), 'r') as fread:
                    f.write(fread.read())
                    fread.close()
                f.write(main_content)
                f.close()
            elif(boardTypes[nodeIndex]=="esp"):
                # Inject command handler registration if needed
                main_content = ""
                with open(os.path.join(base_dir, "codeBlocks/sensors/espMain.c"), 'r') as fread:
                    main_content = fread.read()
                if has_commands:
                    main_content = main_content.replace("sensorInit(&plpc, NULL);", "sensorInit(&plpc, NULL);\n\tregisterCommandHandlers(&plpc);")

                with open(os.path.join(base_dir, "codeBlocks/sensors/espTop.c"), 'r') as fread:
                    f.write(fread.read())
                    top_content = fread.read()
                    if has_commands:
                        top_content = top_content.replace('#include "myDefines.hpp"', '#include "myDefines.hpp"\n#ifdef SENSOR_HAS_COMMANDS\n#include "commandHelper/command_handler.h"\n#endif')
                    f.write(top_content)
                    fread.close()
                localDataIndex = dataIndex - numData[nodeIndex]  # reset localDataIndex for this node
                for frame in ACCESS(node, "CANFrames")["value"]:
                    for dataPoint in ACCESS(frame, "dataInfo")["value"]:
                        f.write("int32_t collect_{0}(bool* cancelFrameSend){{\n    int32_t {0} = {1};\n"
                                "\tmutexPrint(\"collecting {0}\\n\");\n    return {0};\n}}\n\n".format(
                            dataNames[localDataIndex], str(ACCESS(dataPoint, "startingValue")["value"])))
                        localDataIndex += 1
                with open(os.path.join(base_dir, "codeBlocks/sensors/espMain.c"), 'r') as fread:
                    f.write(fread.read())
                    fread.close()
                f.write(main_content)
                f.close()
        
        file_path = os.path.join(sub_dir_path, 'staticDec.cpp')
        # file_path = os.path.join(sub_dir_path, nodeNames[nodeIndex] + 'staticDec.cpp')
        with open(file_path, 'w') as f:

            frameNum = 0
            f.write('#include "myDefines.hpp"\n#include "../common/sensorHelper.hpp"\n\n'
                    '//creates CANFrame array from this node. It stores data to be sent, and info for how to send\n\n')
            for frame in ACCESS(node, "CANFrames")["value"]:
                num_Data = ACCESS(frame, "numData")["value"]
                f.write(f"dataPoint f{frameNum}DataPoints [{num_Data}]={{\n")
                frameNum += 1
                
                for dataPoint in ACCESS(frame, "dataInfo")["value"]:
                    fields = []
                    for field in dataPoint_fields:
                        if "sensor" in field["node"]:
                            value = ACCESS(dataPoint, field["name"])["value"]
                            fields.append(f".{field['name']}={value}")
                    f.write("    {" + ", ".join(fields) + "},\n")
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
                        f.write(f".{field['name']} = {ACCESS(frame, field['name'])['value']}")
                        first = False
                f.write(f", .startingDataIndex={startingDataIndex}")
                startingDataIndex += ACCESS(frame, "numData")["value"]
                f.write(f", .dataInfo=f{frame_index}DataPoints")
                f.write("},\n")
                frame_index += 1
            f.write("};\n")
            f.close()
        nodeIndex += 1

    #Generate platformio.ini environments. Only contains environments for sensor nodes. 
    #Code to be pasted into actual platformio.ini file as an add-on
    file_path = os.path.join(generated_code_dir,'Generatedplatformio.ini')
    with open(file_path, 'w') as f:
        nodeIndex=0
        f.write("\n")
        for node in vitalsNodes:
            if(boardTypes[nodeIndex]=="arduino"):
                f.write(f"[env:{nodeNames[nodeIndex]}]\n")
                f.write("extends=arduinoSensorBase\n")
                f.write(f"build_src_filter = ${{arduinoSensorBase.build_src_filter}}"
                        f"+<sensors/{nodeNames[nodeIndex]}>\n")
                f.write(f"build_flags = -DNODE_CONFIG={nodeNames[nodeIndex]}"
                        "/myDefines.hpp -DSENSOR_ARDUINO_BUILD=ON\n\n")

            elif(boardTypes[nodeIndex]=="esp"):
                f.write(f"[env:{nodeNames[nodeIndex]}]\n")
                f.write("extends=espSensorBase\n")
                f.write(f"board_build.cmake_extra_args = ${{espSensorBase.board_build.cmake_extra_args}}"
                        f" -DSENS_DIR={nodeNames[nodeIndex]}\n")
                f.write(f"build_flags = ${{espSensorBase.build_flags}}"
                         f" -DNODE_CONFIG={nodeNames[nodeIndex]}/myDefines.hpp\n\n")
            nodeIndex+=1
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

        f.write("typedef struct {\n")
        f.write("    const simpleDataPoint* fields;\n    uint8_t num_fields;\n")
        f.write("    uint8_t packet_type;\n")
        f.write("    void (*callback_wrapper)(const uint8_t* raw_packet, size_t packet_len, int8_t bitIndex);\n")
        f.write("} SensorRecvPacketLUTEntry;\n\n")
        f.write(f"extern const int SENSOR_RECV_MASK_BITS;\n")
        f.write(f"extern const SensorRecvPacketLUTEntry sensorRecvPacketLUT[];\n")
        f.write(f"extern const size_t sensorRecvPacketLUTSize;\n\n")
        f.write("#endif // SENSOR_RECV_LUT_H\n")

    # 4. Generate sensorRecvLUT.c
    with open(os.path.join(helper_dir, "sensorRecvLUT.c"), 'w') as f:
        f.write('#include "sensorRecvLUT.h"\n#include <string.h>\n\n')
        for msg in sensor_commands:
            name = msg["name"]
            fields = msg.get("msgFields", [])
            if fields:
                f.write(f"const simpleDataPoint {name}_fields[{len(fields)}] = {{\n")
                for field in fields: f.write(f"    {{ .min={field.min}, .max={field.max}, .bits={field.bits} }},\n") # Corrected order for simpleDataPoint
                f.write("};\n\n")
        
        for msg in sensor_commands:
            name = msg["name"]
            fields = msg.get("msgFields", [])
            byte_count = msg.get("byteCount")
            struct_name = f"{name}_args_t"
            f.write(f"static void {name}_wrapper(const uint8_t* raw_packet, size_t packet_len, int8_t bitIndex) {{\n")
            has_struct = len(fields) > 0 or byte_count is CUSTOM
            if has_struct:
                f.write(f"    {struct_name} args;\n")
                if fields:
                    f.write(f"    int32_t* dest_ptr = (int32_t*)&args;\n")
                    f.write(f"    for (int i = 0; i < {len(fields)}; ++i) {{\n")
                    f.write(f"        pecan_unpack(&dest_ptr[i], raw_packet, &{name}_fields[i], &bitIndex);\n") # bitIndex is already a pointer
                    f.write(f"    }}\n")
                if byte_count is CUSTOM:
                    f.write(f"    size_t fixed_bytes = (bitIndex + 7) / 8;\n")
                    f.write(f"    if (packet_len > fixed_bytes) {{ args.payload = raw_packet + fixed_bytes; args.max_payload_size = packet_len - fixed_bytes; }}")
                    f.write(f" else {{ args.payload = NULL; args.max_payload_size = 0; }}\n")
                f.write(f"    on{name}(args);\n")
            else:
                f.write(f"    on{name}();\n")
            f.write("}\n\n")

        f.write(f"const int SENSOR_RECV_MASK_BITS = {mask_bits};\n")
        f.write("const SensorRecvPacketLUTEntry sensorRecvPacketLUT[] = {\n")
        for msg in sensor_commands:
            f.write(f"    [{msg['mask']}] = {{ // {msg['name']}\n")
            f.write(f"        .fields = {'NULL' if not msg.get('msgFields') else msg['name']+'_fields'},\n")
            f.write(f"        .num_fields = {len(msg.get('msgFields',[]))},\n")
            f.write(f"        .packet_type = SENSOR_RECV_PACKET_TYPE_{'CUSTOM' if msg.get('byteCount') is CUSTOM else 'FIXED'},\n")
            f.write(f"        .callback_wrapper = {msg['name']}_wrapper,\n    }},\n")
        f.write("};\n")
        f.write("const size_t sensorRecvPacketLUTSize = sizeof(sensorRecvPacketLUT) / sizeof(SensorRecvPacketLUTEntry);\n")

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
    with open(os.path.join(helper_dir, "command_handler.c"), 'w') as f:
        f.write('#include "command_handler.h"\n')
        f.write('#include "sensorRecvLUT.h"\n')
        f.write('#include "myDefines.hpp"\n')
        f.write('#include <string.h>\n\n')
        f.write("""
static int16_t handleTelemetryCommand(CANPacket* p) {
    const uint8_t* data = p->data;
    size_t len = p->dataSize;
    
    if (len == 0) return -1;

    int8_t bitIndex = 0;
    int32_t mask_val = 0;
    
    if (SENSOR_RECV_MASK_BITS > 0) {
        simpleDataPoint mask_field = { .bits = SENSOR_RECV_MASK_BITS, .min = 0, .max = 0 };
        pecan_unpack(&mask_val, data, &mask_field, &bitIndex);
    }

    if (mask_val >= sensorRecvPacketLUTSize) {
        return -1; // Invalid mask
    }

    const SensorRecvPacketLUTEntry* entry = &sensorRecvPacketLUT[mask_val];
    if (entry->callback_wrapper) {
        entry->callback_wrapper(data, len, bitIndex);
    }
    return 0;
}

void registerCommandHandlers(PCANListenParamsCollection* plpc) {
    CANListenParam telemCommandParam = {.listen_id = combinedID(TelemetryCommand, myId), .handler = handleTelemetryCommand, .mt = MATCH_EXACT};
    if (addParam(plpc, telemCommandParam) != SUCCESS) {
        // TODO: Handle error, maybe with a print
    }
}
""")

# Note: The ACCESS helper is also defined here to allow local field lookup.
def ACCESS(fields, name):
    return next(field for field in fields if field["name"] == name)
