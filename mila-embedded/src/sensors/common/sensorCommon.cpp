#include "sensorHelper.hpp"
#include "../../pecan/pecan.h"
#include "../../programConstants.h"
#include <stdint.h>
#include <stddef.h> // For size_t
#include <string.h>

// This needs to be included before its macros are used by other declarations
#define STRINGIZE_(a) #a
#define STRINGIZE(a) STRINGIZE_(a)
#include STRINGIZE(NODE_CONFIG)  //includes node Constants

// the list of functions to be called for collecting data. defined in the main file for each sensor
int32_t (*mydataCollectors[node_numData])(bool*) = {dataCollectorsList}; 

void sendFrame(int8_t frameNum) {
    int8_t frameNumData = myframes[frameNum].numData;
    int8_t collectorFuncIndex = myframes[frameNum].startingDataIndex;
    int8_t currBit = 0;
    uint8_t tempData[8] = {0};
    for (int i = 0; i < frameNumData;
         i++) { // iterate over each data. Colect data from dataCollectors, and store compressed version into tempdata.
        bool sendFrame=true; //sendFrame by default
        //Any datapoint can request to cancel sending the entire frame by setting sendFrame to 0
        int32_t data = mydataCollectors[collectorFuncIndex + i](&sendFrame); // collects the data point
        if(sendFrame == false){
            return; //skip sending this frame
        }
        simpleDataPoint* info = &myframes[frameNum].dataInfo[i];
        pecan_pack(&tempData, &currBit, data, info);
    }

    // send the packet
    CANPacket dataPacket;
    memset(&dataPacket, 0, sizeof(CANPacket));
    dataPacket.extendedID = 1;
    dataPacket.id = combinedIDExtended(transmitData, myId, (uint32_t) frameNum);
    writeData(&dataPacket, (int8_t*) tempData, (7 + currBit) / 8);
    sendPacket(&dataPacket);
}


#ifdef SENSOR_HAS_COMMANDS

static int16_t handleTelemetryCommand(CANPacket* p) {
    size_t len = p->dataSize;
    
    if (len == 0) return -1;

    int8_t bitIndex = 0;
    int32_t mask_val = 0;

    if (SENSOR_RECV_MASK_BITS > 0) {
        simpleDataPoint mask_field;
        mask_field.min = 0; 
        mask_field.max = (1U << SENSOR_RECV_MASK_BITS) - 1;
        mask_field.bits = SENSOR_RECV_MASK_BITS;
        pecan_unpack(&mask_val, &p->data, &mask_field, &bitIndex);
    }

    if (mask_val < 0 || mask_val >= sensorRecvPacketLUTSize) {
        return -1; // Invalid mask
    }

    const SensorRecvPacketLUTEntry* entry = &sensorRecvPacketLUT[mask_val];
    if (entry->callback_wrapper) {
        entry->callback_wrapper(p->data, len, &bitIndex);
    }
    return 0;
}

void registerCommandHandler(PCANListenParamsCollection* plpc) {
    CANListenParam telemCommandParam;
    memset(&telemCommandParam, 0, sizeof(telemCommandParam));
    telemCommandParam.listen_id = combinedID(TelemetryCommand, myId);
    telemCommandParam.handler = handleTelemetryCommand;
    telemCommandParam.mt = MATCH_EXACT;
    if (addParam(plpc, telemCommandParam) != SUCCESS) {
        flexiblePrint("Failed to add telemetry command handler");
    }
}

#endif // SENSOR_HAS_COMMANDS
