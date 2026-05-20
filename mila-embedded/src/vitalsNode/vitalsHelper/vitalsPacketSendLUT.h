#ifndef VITALS_PACKET_SEND_LUT_H
#define VITALS_PACKET_SEND_LUT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "pecan/pecan.h"
#include <stddef.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h> // For memcpy

void sendPacketCore(const simpleDataPoint* fields, size_t numFields, const int32_t* data, uint8_t* dataBuffer);
void sendPacketVariable(const simpleDataPoint* fields, size_t numFields, const int32_t* data, const uint8_t* payload, size_t payloadBytes);

// ----- HBTiming -----
typedef struct __attribute__((packed)) sendHBTimingArgs{
    int32_t mask;
    int32_t slowestNode1_ID;
    int32_t slowestNode1_time;
    int32_t slowestNode2_ID;
    int32_t slowestNode2_time;
    int32_t slowestNode3_ID;
    int32_t slowestNode3_time;
} sendHBTimingArgs;

typedef union {
    sendHBTimingArgs s;
    int32_t data_arr[7];
} union_sendHBTiming;

extern const simpleDataPoint HBTiming_fields[7];
static inline void sendHBTimingFunction(sendHBTimingArgs args) {
    args.mask = (int32_t)22; // Auto-assigned
    union_sendHBTiming u __attribute__((aligned(4)));
    u.s = args;
    uint8_t dataBuffer[7] = {0};
    sendPacketCore(HBTiming_fields, 7, u.data_arr, dataBuffer);
}

// ----- HBStatus -----
typedef struct __attribute__((packed)) sendHBStatusArgs{
    int32_t mask;
    int32_t HBMask;
} sendHBStatusArgs;

typedef union {
    sendHBStatusArgs s;
    int32_t data_arr[2];
} union_sendHBStatus;

extern const simpleDataPoint HBStatus_fields[2];
static inline void sendHBStatusFunction(sendHBStatusArgs args) {
    args.mask = (int32_t)4; // Auto-assigned
    union_sendHBStatus u __attribute__((aligned(4)));
    u.s = args;
    uint8_t dataBuffer[1] = {0};
    sendPacketCore(HBStatus_fields, 2, u.data_arr, dataBuffer);
}

// ----- BusStatus -----
typedef union {
    int32_t i32;
    TWAI_STATE e;
} union_BusStatus_TWAI_STATE;

typedef struct __attribute__((packed)) sendBusStatusArgs{
    int32_t mask;
    union_BusStatus_TWAI_STATE TWAI_STATE;
    int32_t TWAI_TX_Err_Cnt;
    int32_t TWAI_RX_Err_Cnt;
    int32_t TWAI_Err_Cnt;
    int32_t failed_TX_Cnt;
    int32_t RX_Overrun_Cnt;
    int32_t RX_Missed_Cnt;
    int32_t RX_Recv_Queue_Cnt;
} sendBusStatusArgs;

typedef union {
    sendBusStatusArgs s;
    int32_t data_arr[9];
} union_sendBusStatus;

extern const simpleDataPoint BusStatus_fields[9];
static inline void sendBusStatusFunction(sendBusStatusArgs args) {
    args.mask = (int32_t)10; // Auto-assigned
    union_sendBusStatus u __attribute__((aligned(4)));
    u.s = args;
    uint8_t dataBuffer[9] = {0};
    sendPacketCore(BusStatus_fields, 9, u.data_arr, dataBuffer);
}

// ----- vitalsErr -----
typedef struct __attribute__((packed)) sendvitalsErrHeader{
    int32_t mask;
    int32_t numErrors;
} sendvitalsErrHeader;

typedef union {
    sendvitalsErrHeader s;
    int32_t data_arr[2];
} union_sendvitalsErr;

extern const simpleDataPoint vitalsErr_fields[2];
static inline void sendvitalsErrFunction(sendvitalsErrHeader header, const uint8_t* payload, size_t payloadBytes) {
    header.mask = (int32_t)191; // Auto-assigned
    union_sendvitalsErr u __attribute__((aligned(4)));
    u.s = header;
    sendPacketVariable(vitalsErr_fields, 2, u.data_arr, payload, payloadBytes);
}

// ----- dataWarning -----
typedef union {
    int32_t i32;
    errorTrigger e;
} union_dataWarning_errorTrigger;

typedef struct __attribute__((packed)) senddataWarningArgs{
    int32_t mask;
    int32_t data_too_high;
    int32_t extrapolationDueToTimeout;
    union_dataWarning_errorTrigger errorTrigger;
    int32_t nodeID;
    int32_t frameID;
    int32_t dataID;
} senddataWarningArgs;

typedef union {
    senddataWarningArgs s;
    int32_t data_arr[7];
} union_senddataWarning;

extern const simpleDataPoint dataWarning_fields[7];
static inline void senddataWarningFunction(senddataWarningArgs args) {
    args.mask = (int32_t)46; // Auto-assigned
    union_senddataWarning u __attribute__((aligned(4)));
    u.s = args;
    uint8_t dataBuffer[3] = {0};
    sendPacketCore(dataWarning_fields, 7, u.data_arr, dataBuffer);
}

// ----- nodeStatus -----
typedef union {
    int32_t i32;
    statusUpdates e;
} union_nodeStatus_statusUpdates;

typedef struct __attribute__((packed)) sendnodeStatusArgs{
    int32_t mask;
    int32_t nodeID;
    union_nodeStatus_statusUpdates statusUpdates;
} sendnodeStatusArgs;

typedef union {
    sendnodeStatusArgs s;
    int32_t data_arr[3];
} union_sendnodeStatus;

extern const simpleDataPoint nodeStatus_fields[3];
static inline void sendnodeStatusFunction(sendnodeStatusArgs args) {
    args.mask = (int32_t)94; // Auto-assigned
    union_sendnodeStatus u __attribute__((aligned(4)));
    u.s = args;
    uint8_t dataBuffer[2] = {0};
    sendPacketCore(nodeStatus_fields, 3, u.data_arr, dataBuffer);
}

// ----- unknownCanPacket -----
extern const simpleDataPoint unknownCanPacket_fields[1];
static inline void sendunknownCanPacketFunction(const uint8_t* payload, size_t payloadBytes) {
    int32_t data[1] = {(int32_t)190};
    sendPacketVariable(unknownCanPacket_fields, 1, data, payload, payloadBytes);
}

// ----- CANDataFrame -----
typedef struct __attribute__((packed)) sendCANDataFrameHeader{
    int32_t mask;
    int32_t nodeID;
} sendCANDataFrameHeader;

typedef union {
    sendCANDataFrameHeader s;
    int32_t data_arr[2];
} union_sendCANDataFrame;

extern const simpleDataPoint CANDataFrame_fields[2];
static inline void sendCANDataFrameFunction(sendCANDataFrameHeader header, const uint8_t* payload, size_t payloadBytes) {
    header.mask = (int32_t)0; // Auto-assigned
    union_sendCANDataFrame u __attribute__((aligned(4)));
    u.s = header;
    sendPacketVariable(CANDataFrame_fields, 2, u.data_arr, payload, payloadBytes);
}


#ifdef __cplusplus
}
#endif

#endif // VITALS_PACKET_SEND_LUT_H
