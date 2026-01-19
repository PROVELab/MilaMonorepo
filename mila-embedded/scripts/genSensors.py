import os
from parseFile import dataPoint_fields, CANFrame_fields, ACCESS

def createSensors(vitalsNodes, nodeNames, boardTypes, nodeIds, dataNames, numData, base_dir, generated_code_dir):

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

        # write the dataPoints struct (for sensors):
        f.write("typedef struct{\n")
        for field in dataPoint_fields:
            if "sensor" in field["node"]:
                f.write("    " + field["type"] + " " + field["name"] + ";\n")

        f.write("} dataPoint;\n\n")
        # write the CANFrame struct
        f.write("typedef struct{    //identified by a 2 bit identifier 0-3 in function code\n")
        for field in CANFrame_fields:
            if "sensor" in field["node"]:
                f.write("    " + field["type"] + " " + field["name"] + ";\n")     
        # custom fields here
        f.write("    int8_t startingDataIndex;  //starting index of data in this frame. used by collector function\n")
        f.write("    dataPoint *dataInfo;\n")
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
            # includes
            f.write('#ifndef ' + nodeNames[nodeIndex] + '_DATA_H\n#define ' + nodeNames[nodeIndex] + '_DATA_H\n')
            f.write("//defines constants specific to " + nodeNames[nodeIndex])
            f.write('#include "../common/sensorHelper.hpp"\n#include<stdint.h>\n')
            f.write("#define myId " + str(nodeIds[nodeIndex]))
            f.write("\n#define numFrames " + str(ACCESS(node, "numFrames")["value"]))
            f.write("\n#define node_numData " + str(numData[nodeIndex]) + "\n\n")
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
        with open(file_path, 'w') as f:
            if(boardTypes[nodeIndex]=="arduino"):    #create main.cpp for arduino sensors
                with open(os.path.join(base_dir, "codeBlocks/sensors/arduinoTop.cpp"), 'r') as fread:
                    f.write(fread.read())
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
                f.close()
            elif(boardTypes[nodeIndex]=="esp"):
                with open(os.path.join(base_dir, "codeBlocks/sensors/espTop.c"), 'r') as fread:
                    f.write(fread.read())
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


# Note: The ACCESS helper is also defined here to allow local field lookup.
def ACCESS(fields, name):
    return next(field for field in fields if field["name"] == name)
