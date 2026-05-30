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

// Rate limiting for vitals-to-telemetry packets
typedef struct {
    uint8_t divider; // Send every Nth call. 1 = send every time.
    uint8_t counter; // Internal counter.
} VitalsSendRateController;

#include "../../programConstants.h" // For numVitalsToTelemPackets
extern VitalsSendRateController vitals_send_rate_controllers[numVitalsToTelemPackets];

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
static inline void sendHBTimingFunction(sendHBTimingArgs args); // Forward declaration
static inline void sendHBTimingFunction_internal(sendHBTimingArgs args) {
    args.mask = (int32_t)34; // Auto-assigned
    union_sendHBTiming u __attribute__((aligned(4)));
    u.s = args;
    uint8_t dataBuffer[6] = {0};
    sendPacketCore(HBTiming_fields, 7, u.data_arr, dataBuffer);
}

static inline void sendHBTimingFunction(sendHBTimingArgs args) {
    if (++vitals_send_rate_controllers[0].counter < vitals_send_rate_controllers[0].divider) return;
    vitals_send_rate_controllers[0].counter = 0;
    sendHBTimingFunction_internal(args);
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
static inline void sendHBStatusFunction(sendHBStatusArgs args); // Forward declaration
static inline void sendHBStatusFunction_internal(sendHBStatusArgs args) {
    args.mask = (int32_t)2; // Auto-assigned
    union_sendHBStatus u __attribute__((aligned(4)));
    u.s = args;
    uint8_t dataBuffer[1] = {0};
    sendPacketCore(HBStatus_fields, 2, u.data_arr, dataBuffer);
}

static inline void sendHBStatusFunction(sendHBStatusArgs args) {
    if (++vitals_send_rate_controllers[1].counter < vitals_send_rate_controllers[1].divider) return;
    vitals_send_rate_controllers[1].counter = 0;
    sendHBStatusFunction_internal(args);
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
static inline void sendBusStatusFunction(sendBusStatusArgs args); // Forward declaration
static inline void sendBusStatusFunction_internal(sendBusStatusArgs args) {
    args.mask = (int32_t)6; // Auto-assigned
    union_sendBusStatus u __attribute__((aligned(4)));
    u.s = args;
    uint8_t dataBuffer[9] = {0};
    sendPacketCore(BusStatus_fields, 9, u.data_arr, dataBuffer);
}

static inline void sendBusStatusFunction(sendBusStatusArgs args) {
    if (++vitals_send_rate_controllers[2].counter < vitals_send_rate_controllers[2].divider) return;
    vitals_send_rate_controllers[2].counter = 0;
    sendBusStatusFunction_internal(args);
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
static inline void sendvitalsErrFunction(sendvitalsErrHeader header, const uint8_t* payload, size_t payloadBytes); // Forward declaration
static inline void sendvitalsErrFunction_internal(sendvitalsErrHeader header, const uint8_t* payload, size_t payloadBytes) {
    header.mask = (int32_t)140; // Auto-assigned
    union_sendvitalsErr u __attribute__((aligned(4)));
    u.s = header;
    sendPacketVariable(vitalsErr_fields, 2, u.data_arr, payload, payloadBytes);
}

static inline void sendvitalsErrFunction(sendvitalsErrHeader header, const uint8_t* payload, size_t payloadBytes) {
    if (++vitals_send_rate_controllers[3].counter < vitals_send_rate_controllers[3].divider) return;
    vitals_send_rate_controllers[3].counter = 0;
    sendvitalsErrFunction_internal(header, payload, payloadBytes);
}

// ----- dataWarning -----
typedef union {
    int32_t i32;
    dataErrorTrigger e;
} union_dataWarning_dataErrorTrigger;

typedef struct __attribute__((packed)) senddataWarningArgs{
    int32_t mask;
    int32_t dataTooHigh;
    union_dataWarning_dataErrorTrigger dataErrorTrigger;
    int32_t nodeID;
    int32_t frameID;
    int32_t dataID;
} senddataWarningArgs;

typedef union {
    senddataWarningArgs s;
    int32_t data_arr[6];
} union_senddataWarning;

extern const simpleDataPoint dataWarning_fields[6];
static inline void senddataWarningFunction(senddataWarningArgs args); // Forward declaration
static inline void senddataWarningFunction_internal(senddataWarningArgs args) {
    args.mask = (int32_t)564; // Auto-assigned
    union_senddataWarning u __attribute__((aligned(4)));
    u.s = args;
    uint8_t dataBuffer[3] = {0};
    sendPacketCore(dataWarning_fields, 6, u.data_arr, dataBuffer);
}

static inline void senddataWarningFunction(senddataWarningArgs args) {
    if (++vitals_send_rate_controllers[4].counter < vitals_send_rate_controllers[4].divider) return;
    vitals_send_rate_controllers[4].counter = 0;
    senddataWarningFunction_internal(args);
}

// ----- frameWarning -----
typedef union {
    int32_t i32;
    frameErrorTrigger e;
} union_frameWarning_frameErrorTrigger;

typedef struct __attribute__((packed)) sendframeWarningArgs{
    int32_t mask;
    union_frameWarning_frameErrorTrigger frameErrorTrigger;
    int32_t nodeID;
    int32_t frameID;
} sendframeWarningArgs;

typedef union {
    sendframeWarningArgs s;
    int32_t data_arr[4];
} union_sendframeWarning;

extern const simpleDataPoint frameWarning_fields[4];
static inline void sendframeWarningFunction(sendframeWarningArgs args); // Forward declaration
static inline void sendframeWarningFunction_internal(sendframeWarningArgs args) {
    args.mask = (int32_t)36160; // Auto-assigned
    union_sendframeWarning u __attribute__((aligned(4)));
    u.s = args;
    uint8_t dataBuffer[3] = {0};
    sendPacketCore(frameWarning_fields, 4, u.data_arr, dataBuffer);
}

static inline void sendframeWarningFunction(sendframeWarningArgs args) {
    if (++vitals_send_rate_controllers[5].counter < vitals_send_rate_controllers[5].divider) return;
    vitals_send_rate_controllers[5].counter = 0;
    sendframeWarningFunction_internal(args);
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
static inline void sendnodeStatusFunction(sendnodeStatusArgs args); // Forward declaration
static inline void sendnodeStatusFunction_internal(sendnodeStatusArgs args) {
    args.mask = (int32_t)0; // Auto-assigned
    union_sendnodeStatus u __attribute__((aligned(4)));
    u.s = args;
    uint8_t dataBuffer[1] = {0};
    sendPacketCore(nodeStatus_fields, 3, u.data_arr, dataBuffer);
}

static inline void sendnodeStatusFunction(sendnodeStatusArgs args) {
    if (++vitals_send_rate_controllers[6].counter < vitals_send_rate_controllers[6].divider) return;
    vitals_send_rate_controllers[6].counter = 0;
    sendnodeStatusFunction_internal(args);
}

// ----- unknownCanPacket -----
typedef struct __attribute__((packed)) sendunknownCanPacketHeader{
    int32_t mask;
    int32_t nodeID;
    int32_t DLC;
    int32_t extendedIDPresent;
    int32_t RTR;
    int32_t ext_id_start;
} sendunknownCanPacketHeader;

typedef union {
    sendunknownCanPacketHeader s;
    int32_t data_arr[6];
} union_sendunknownCanPacket;

extern const simpleDataPoint unknownCanPacket_fields[6];
static inline void sendunknownCanPacketFunction(sendunknownCanPacketHeader header, const uint8_t* payload, size_t payloadBytes); // Forward declaration
static inline void sendunknownCanPacketFunction_internal(sendunknownCanPacketHeader header, const uint8_t* payload, size_t payloadBytes) {
    header.mask = (int32_t)16; // Auto-assigned
    union_sendunknownCanPacket u __attribute__((aligned(4)));
    u.s = header;
    sendPacketVariable(unknownCanPacket_fields, 6, u.data_arr, payload, payloadBytes);
}

static inline void sendunknownCanPacketFunction(sendunknownCanPacketHeader header, const uint8_t* payload, size_t payloadBytes) {
    if (++vitals_send_rate_controllers[7].counter < vitals_send_rate_controllers[7].divider) return;
    vitals_send_rate_controllers[7].counter = 0;
    sendunknownCanPacketFunction_internal(header, payload, payloadBytes);
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
static inline void sendCANDataFrameFunction(sendCANDataFrameHeader header, const uint8_t* payload, size_t payloadBytes); // Forward declaration
static inline void sendCANDataFrameFunction_internal(sendCANDataFrameHeader header, const uint8_t* payload, size_t payloadBytes) {
    header.mask = (int32_t)7; // Auto-assigned
    union_sendCANDataFrame u __attribute__((aligned(4)));
    u.s = header;
    sendPacketVariable(CANDataFrame_fields, 2, u.data_arr, payload, payloadBytes);
}

static inline void sendCANDataFrameFunction(sendCANDataFrameHeader header, const uint8_t* payload, size_t payloadBytes) {
    if (++vitals_send_rate_controllers[8].counter < vitals_send_rate_controllers[8].divider) return;
    vitals_send_rate_controllers[8].counter = 0;
    sendCANDataFrameFunction_internal(header, payload, payloadBytes);
}


#ifdef __cplusplus
}
#endif

#endif // VITALS_PACKET_SEND_LUT_H
