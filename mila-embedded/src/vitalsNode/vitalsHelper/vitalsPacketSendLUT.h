#ifndef VITALS_PACKET_SEND_LUT_H
#define VITALS_PACKET_SEND_LUT_H

#include "vitalsStructs.h"
#include <stddef.h>
#include <stdint.h>

void sendPacketCore(const simpleDataPoint* fields, size_t numFields, const int* data, uint8_t* tempData, size_t tempBytes);

// ----- HBTiming -----
extern const simpleDataPoint HBTiming_fields[7];
inline void sendHBTimingFunction(int32_t d0, int32_t d1, int32_t d2, int32_t d3, int32_t d4, int32_t d5) {
    int32_t data[7] = {(int32_t)8U, d0, d1, d2, d3, d4, d5};
    uint8_t tempData[7] = {0};
    sendPacketCore(HBTiming_fields, 7, data, tempData, 7);
}

// ----- HBStatus -----
extern const simpleDataPoint HBStatus_fields[2];
inline void sendHBStatusFunction(int32_t d0) {
    int32_t data[2] = {(int32_t)9U, d0};
    uint8_t tempData[1] = {0};
    sendPacketCore(HBStatus_fields, 2, data, tempData, 1);
}

// ----- BusStatus -----
extern const simpleDataPoint BusStatus_fields[9];
inline void sendBusStatusFunction(int32_t d0, int32_t d1, int32_t d2, int32_t d3, int32_t d4, int32_t d5, int32_t d6, int32_t d7) {
    int32_t data[9] = {(int32_t)10U, d0, d1, d2, d3, d4, d5, d6, d7};
    uint8_t tempData[9] = {0};
    sendPacketCore(BusStatus_fields, 9, data, tempData, 9);
}

// ----- vitalsErr -----
void sendvitalsErrFunction(const uint8_t* data, size_t size);

// ----- dataWarning -----
extern const simpleDataPoint dataWarning_fields[7];
inline void senddataWarningFunction(int32_t d0, int32_t d1, int32_t d2, int32_t d3, int32_t d4, int32_t d5) {
    int32_t data[7] = {(int32_t)12U, d0, d1, d2, d3, d4, d5};
    uint8_t tempData[2] = {0};
    sendPacketCore(dataWarning_fields, 7, data, tempData, 2);
}

// ----- nodeStatus -----
extern const simpleDataPoint nodeStatus_fields[3];
inline void sendnodeStatusFunction(int32_t d0, int32_t d1) {
    int32_t data[3] = {(int32_t)13U, d0, d1};
    uint8_t tempData[2] = {0};
    sendPacketCore(nodeStatus_fields, 3, data, tempData, 2);
}

// ----- unknownCanPacket -----
void sendunknownCanPacketFunction(const uint8_t* data, size_t size);

// ----- CANDataFrame -----
void sendCANDataFrameFunction(const uint8_t* data, size_t size);

#endif // VITALS_PACKET_SEND_LUT_H
