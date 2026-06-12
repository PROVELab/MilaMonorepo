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

#include "../vitalsSendData.h"
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
static inline uint8_t formatHBTimingFunction(sendHBTimingArgs args, uint8_t* out_buffer) {
    args.mask = (int32_t)17; // Auto-assigned
    union_sendHBTiming u __attribute__((aligned(4)));
    u.s = args;
    return formatPacketCore(HBTiming_fields, 7, u.data_arr, out_buffer);
}

static inline void sendHBTimingFunction(sendHBTimingArgs args) {
    if (++vitals_send_rate_controllers[0].counter < vitals_send_rate_controllers[0].divider) return;
    vitals_send_rate_controllers[0].counter = 0;
    uint8_t dataBuffer[6] = {0};
    args.mask = (int32_t)17; // Auto-assigned
    union_sendHBTiming u __attribute__((aligned(4)));
    u.s = args;
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
static inline uint8_t formatHBStatusFunction(sendHBStatusArgs args, uint8_t* out_buffer) {
    args.mask = (int32_t)2; // Auto-assigned
    union_sendHBStatus u __attribute__((aligned(4)));
    u.s = args;
    return formatPacketCore(HBStatus_fields, 2, u.data_arr, out_buffer);
}

static inline void sendHBStatusFunction(sendHBStatusArgs args) {
    if (++vitals_send_rate_controllers[1].counter < vitals_send_rate_controllers[1].divider) return;
    vitals_send_rate_controllers[1].counter = 0;
    uint8_t dataBuffer[1] = {0};
    args.mask = (int32_t)2; // Auto-assigned
    union_sendHBStatus u __attribute__((aligned(4)));
    u.s = args;
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
static inline uint8_t formatBusStatusFunction(sendBusStatusArgs args, uint8_t* out_buffer) {
    args.mask = (int32_t)6; // Auto-assigned
    union_sendBusStatus u __attribute__((aligned(4)));
    u.s = args;
    return formatPacketCore(BusStatus_fields, 9, u.data_arr, out_buffer);
}

static inline void sendBusStatusFunction(sendBusStatusArgs args) {
    if (++vitals_send_rate_controllers[2].counter < vitals_send_rate_controllers[2].divider) return;
    vitals_send_rate_controllers[2].counter = 0;
    uint8_t dataBuffer[9] = {0};
    args.mask = (int32_t)6; // Auto-assigned
    union_sendBusStatus u __attribute__((aligned(4)));
    u.s = args;
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
static inline uint8_t formatvitalsErrFunction(sendvitalsErrHeader header, const uint8_t* payload, size_t payloadBytes, uint8_t* out_buffer) {
    header.mask = (int32_t)49; // Auto-assigned
    union_sendvitalsErr u __attribute__((aligned(4)));
    u.s = header;
    return formatPacketVariable(vitalsErr_fields, 2, u.data_arr, payload, payloadBytes, out_buffer);
}

static inline void sendvitalsErrFunction(sendvitalsErrHeader header, const uint8_t* payload, size_t payloadBytes) {
    if (++vitals_send_rate_controllers[3].counter < vitals_send_rate_controllers[3].divider) return;
    vitals_send_rate_controllers[3].counter = 0;
    header.mask = (int32_t)49; // Auto-assigned
    union_sendvitalsErr u __attribute__((aligned(4)));
    u.s = header;
    sendPacketVariable(vitalsErr_fields, 2, u.data_arr, payload, payloadBytes);
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
static inline uint8_t formatdataWarningFunction(senddataWarningArgs args, uint8_t* out_buffer) {
    args.mask = (int32_t)177; // Auto-assigned
    union_senddataWarning u __attribute__((aligned(4)));
    u.s = args;
    return formatPacketCore(dataWarning_fields, 6, u.data_arr, out_buffer);
}

static inline void senddataWarningFunction(senddataWarningArgs args) {
    if (++vitals_send_rate_controllers[4].counter < vitals_send_rate_controllers[4].divider) return;
    vitals_send_rate_controllers[4].counter = 0;
    uint8_t dataBuffer[3] = {0};
    args.mask = (int32_t)177; // Auto-assigned
    union_senddataWarning u __attribute__((aligned(4)));
    u.s = args;
    sendPacketCore(dataWarning_fields, 6, u.data_arr, dataBuffer);
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
static inline uint8_t formatframeWarningFunction(sendframeWarningArgs args, uint8_t* out_buffer) {
    args.mask = (int32_t)689; // Auto-assigned
    union_sendframeWarning u __attribute__((aligned(4)));
    u.s = args;
    return formatPacketCore(frameWarning_fields, 4, u.data_arr, out_buffer);
}

static inline void sendframeWarningFunction(sendframeWarningArgs args) {
    if (++vitals_send_rate_controllers[5].counter < vitals_send_rate_controllers[5].divider) return;
    vitals_send_rate_controllers[5].counter = 0;
    uint8_t dataBuffer[3] = {0};
    args.mask = (int32_t)689; // Auto-assigned
    union_sendframeWarning u __attribute__((aligned(4)));
    u.s = args;
    sendPacketCore(frameWarning_fields, 4, u.data_arr, dataBuffer);
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
static inline uint8_t formatnodeStatusFunction(sendnodeStatusArgs args, uint8_t* out_buffer) {
    args.mask = (int32_t)0; // Auto-assigned
    union_sendnodeStatus u __attribute__((aligned(4)));
    u.s = args;
    return formatPacketCore(nodeStatus_fields, 3, u.data_arr, out_buffer);
}

static inline void sendnodeStatusFunction(sendnodeStatusArgs args) {
    if (++vitals_send_rate_controllers[6].counter < vitals_send_rate_controllers[6].divider) return;
    vitals_send_rate_controllers[6].counter = 0;
    uint8_t dataBuffer[1] = {0};
    args.mask = (int32_t)0; // Auto-assigned
    union_sendnodeStatus u __attribute__((aligned(4)));
    u.s = args;
    sendPacketCore(nodeStatus_fields, 3, u.data_arr, dataBuffer);
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
static inline uint8_t formatunknownCanPacketFunction(sendunknownCanPacketHeader header, const uint8_t* payload, size_t payloadBytes, uint8_t* out_buffer) {
    header.mask = (int32_t)1; // Auto-assigned
    union_sendunknownCanPacket u __attribute__((aligned(4)));
    u.s = header;
    return formatPacketVariable(unknownCanPacket_fields, 6, u.data_arr, payload, payloadBytes, out_buffer);
}

static inline void sendunknownCanPacketFunction(sendunknownCanPacketHeader header, const uint8_t* payload, size_t payloadBytes) {
    if (++vitals_send_rate_controllers[7].counter < vitals_send_rate_controllers[7].divider) return;
    vitals_send_rate_controllers[7].counter = 0;
    header.mask = (int32_t)1; // Auto-assigned
    union_sendunknownCanPacket u __attribute__((aligned(4)));
    u.s = header;
    sendPacketVariable(unknownCanPacket_fields, 6, u.data_arr, payload, payloadBytes);
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
static inline uint8_t formatCANDataFrameFunction(sendCANDataFrameHeader header, const uint8_t* payload, size_t payloadBytes, uint8_t* out_buffer) {
    header.mask = (int32_t)14; // Auto-assigned
    union_sendCANDataFrame u __attribute__((aligned(4)));
    u.s = header;
    return formatPacketVariable(CANDataFrame_fields, 2, u.data_arr, payload, payloadBytes, out_buffer);
}

static inline void sendCANDataFrameFunction(sendCANDataFrameHeader header, const uint8_t* payload, size_t payloadBytes) {
    if (++vitals_send_rate_controllers[8].counter < vitals_send_rate_controllers[8].divider) return;
    vitals_send_rate_controllers[8].counter = 0;
    header.mask = (int32_t)14; // Auto-assigned
    union_sendCANDataFrame u __attribute__((aligned(4)));
    u.s = header;
    sendPacketVariable(CANDataFrame_fields, 2, u.data_arr, payload, payloadBytes);
}


#ifdef __cplusplus
}
#endif

#endif // VITALS_PACKET_SEND_LUT_H
