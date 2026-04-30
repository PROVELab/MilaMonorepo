#ifndef VITALS_PACKET_SEND_LUT_H
#define VITALS_PACKET_SEND_LUT_H

#include "vitalsStructs.h"
#include <stddef.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

void sendPacketCore(const simpleDataPoint* fields, size_t numFields, const int* data, uint8_t* dataBuffer);
void sendPacketVariable(const simpleDataPoint* fields, size_t numFields, const int* data, const uint8_t* payload, const uint8_t payloadBytes);

// ----- HBTiming -----
typedef struct __attribute__((packed)) sendHBTimingArgs{
    int32_t slowestNode1_ID;
    int32_t slowestNode1_time;
    int32_t slowestNode2_ID;
    int32_t slowestNode2_time;
    int32_t slowestNode3_ID;
    int32_t slowestNode3_time;
} sendHBTimingArgs;

extern const simpleDataPoint HBTiming_fields[7];
inline void sendHBTimingFunction(sendHBTimingArgs args) {

    int32_t* data = (int32_t*) &args; 
    uint8_t dataBuffer[7] = {0};
    sendPacketCore(HBTiming_fields, 7, data, dataBuffer, 7);
}

// ----- HBStatus -----
typedef struct __attribute__((packed)) sendHBStatusArgs{
    int32_t HBMask;
} sendHBStatusArgs;

extern const simpleDataPoint HBStatus_fields[2];
inline void sendHBStatusFunction(sendHBStatusArgs args) {

    int32_t* data = (int32_t*) &args; 
    uint8_t dataBuffer[1] = {0};
    sendPacketCore(HBStatus_fields, 2, data, dataBuffer, 1);
}

// ----- BusStatus -----
typedef struct __attribute__((packed)) sendBusStatusArgs{
    int32_t TWAI_STATE;
    int32_t TWAI_TX_Err_Cnt;
    int32_t TWAI_RX_Err_Cnt;
    int32_t TWAI_Err_Cnt;
    int32_t failed_TX_Cnt;
    int32_t RX_Overrun_Cnt;
    int32_t RX_Missed_Cnt;
    int32_t RX_Recv_Queue_Cnt;
} sendBusStatusArgs;

extern const simpleDataPoint BusStatus_fields[9];
inline void sendBusStatusFunction(sendBusStatusArgs args) {

    int32_t* data = (int32_t*) &args; 
    uint8_t dataBuffer[9] = {0};
    sendPacketCore(BusStatus_fields, 9, data, dataBuffer, 9);
}

// ----- vitalsErr -----
typedef struct __attribute__((packed)) sendvitalsErrArgs{
    int32_t numErrors;
    uint8_t* payload;
    size_t payloadBytes;
} sendvitalsErrArgs;

extern const simpleDataPoint vitalsErr_fields[2];
inline void sendvitalsErrFunction(sendvitalsErrArgs args) {

    int32_t* data = (int32_t*) &args; 
    sendPacketVariable(vitalsErr_fields, 2, data, args.payload, args.payloadBytes);
}

// ----- dataWarning -----
typedef struct __attribute__((packed)) senddataWarningArgs{
    int32_t isCritical;
    int32_t data_too_high;
    int32_t extrapolationTrigger;
    int32_t nodeID;
    int32_t frameID;
    int32_t dataID;
} senddataWarningArgs;

extern const simpleDataPoint dataWarning_fields[7];
inline void senddataWarningFunction(senddataWarningArgs args) {

    int32_t* data = (int32_t*) &args; 
    uint8_t dataBuffer[2] = {0};
    sendPacketCore(dataWarning_fields, 7, data, dataBuffer, 2);
}

// ----- nodeStatus -----
typedef struct __attribute__((packed)) sendnodeStatusArgs{
    int32_t nodeID;
    int32_t statusUpdates;
} sendnodeStatusArgs;

extern const simpleDataPoint nodeStatus_fields[3];
inline void sendnodeStatusFunction(sendnodeStatusArgs args) {

    int32_t* data = (int32_t*) &args; 
    uint8_t dataBuffer[2] = {0};
    sendPacketCore(nodeStatus_fields, 3, data, dataBuffer, 2);
}

// ----- unknownCanPacket -----
extern const simpleDataPoint unknownCanPacket_fields[1];
inline void sendunknownCanPacketFunction(const uint8_t* payload, size_t payloadBytes) {

    sendPacketVariable(unknownCanPacket_fields, 1, NULL, payload, payloadBytes);
}

// ----- CANDataFrame -----
typedef struct __attribute__((packed)) sendCANDataFrameArgs{
    int32_t nodeID;
    uint8_t* payload;
    size_t payloadBytes;
} sendCANDataFrameArgs;

extern const simpleDataPoint CANDataFrame_fields[2];
inline void sendCANDataFrameFunction(sendCANDataFrameArgs args) {

    int32_t* data = (int32_t*) &args; 
    sendPacketVariable(CANDataFrame_fields, 2, data, args.payload, args.payloadBytes);
}

#endif // VITALS_PACKET_SEND_LUT_H
