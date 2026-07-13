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
    volatile uint8_t divider; // Send every Nth call. 1 = send every time.                 volatile for convenient async updates
    uint8_t counter; // Internal counter.
} VitalsSendRateController;

#include "../../programConstants.h" // For numVitalsToTelemPackets
extern VitalsSendRateController vitals_send_rate_controllers[numVitalsToTelemPackets];

#include "../loraStaticHelper/staticHelp.h"
// ----- VitalsUpdate -----
typedef union {
    int32_t i32;
    vitalsContactorState e;
} union_VitalsUpdate_vitalsContactorState;

typedef union {
    int32_t i32;
    TWAI_STATE e;
} union_VitalsUpdate_TWAI_STATE;

typedef struct __attribute__((packed)) sendVitalsUpdateArgs{
    int32_t mask;
    union_VitalsUpdate_vitalsContactorState vitalsContactorState;
    int32_t contactorStateLatched;
    int32_t TWAI_TX_Err_Cnt;
    int32_t TWAI_RX_Err_Cnt;
    int32_t TWAI_Err_Cnt;
    int32_t failed_TX_Cnt;
    int32_t RX_Overrun_Cnt;
    int32_t RX_Missed_Cnt;
    int32_t RX_Recv_Queue_Cnt;
    int32_t slowestNode1_ID;
    int32_t slowestNode1_time;
    int32_t slowestNode2_ID;
    int32_t slowestNode2_time;
    int32_t slowestNode3_ID;
    int32_t slowestNode3_time;
    union_VitalsUpdate_TWAI_STATE TWAI_STATE;
    int32_t HBMask;
} sendVitalsUpdateArgs;

typedef union {
    sendVitalsUpdateArgs s;
    int32_t data_arr[18];
} union_sendVitalsUpdate;

extern const simpleDataPoint VitalsUpdate_fields[18];
static inline uint8_t formatVitalsUpdateFunction(sendVitalsUpdateArgs args, uint8_t* out_buffer) {
    args.mask = (int32_t)2; // Auto-assigned
    union_sendVitalsUpdate u __attribute__((aligned(4)));
    u.s = args;
    return formatPacketCore(VitalsUpdate_fields, 18, u.data_arr, out_buffer);
}

static inline void sendVitalsUpdateFunction(sendVitalsUpdateArgs args) {
    if (++vitals_send_rate_controllers[0].counter < vitals_send_rate_controllers[0].divider) return;
    vitals_send_rate_controllers[0].counter = 0;
    uint8_t dataBuffer[15] = {0};
    args.mask = (int32_t)2; // Auto-assigned
    union_sendVitalsUpdate u __attribute__((aligned(4)));
    u.s = args;
    sendPacketCore(VitalsUpdate_fields, 18, u.data_arr, dataBuffer);
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
    header.mask = (int32_t)17; // Auto-assigned
    union_sendvitalsErr u __attribute__((aligned(4)));
    u.s = header;
    return formatPacketVariable(vitalsErr_fields, 2, u.data_arr, payload, payloadBytes, out_buffer);
}

static inline void sendvitalsErrFunction(sendvitalsErrHeader header, const uint8_t* payload, size_t payloadBytes) {
    if (++vitals_send_rate_controllers[1].counter < vitals_send_rate_controllers[1].divider) return;
    vitals_send_rate_controllers[1].counter = 0;
    header.mask = (int32_t)17; // Auto-assigned
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
    args.mask = (int32_t)401; // Auto-assigned
    union_senddataWarning u __attribute__((aligned(4)));
    u.s = args;
    return formatPacketCore(dataWarning_fields, 6, u.data_arr, out_buffer);
}

static inline void senddataWarningFunction(senddataWarningArgs args) {
    if (++vitals_send_rate_controllers[2].counter < vitals_send_rate_controllers[2].divider) return;
    vitals_send_rate_controllers[2].counter = 0;
    uint8_t dataBuffer[3] = {0};
    args.mask = (int32_t)401; // Auto-assigned
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
    args.mask = (int32_t)913; // Auto-assigned
    union_sendframeWarning u __attribute__((aligned(4)));
    u.s = args;
    return formatPacketCore(frameWarning_fields, 4, u.data_arr, out_buffer);
}

static inline void sendframeWarningFunction(sendframeWarningArgs args) {
    if (++vitals_send_rate_controllers[3].counter < vitals_send_rate_controllers[3].divider) return;
    vitals_send_rate_controllers[3].counter = 0;
    uint8_t dataBuffer[3] = {0};
    args.mask = (int32_t)913; // Auto-assigned
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
    args.mask = (int32_t)145; // Auto-assigned
    union_sendnodeStatus u __attribute__((aligned(4)));
    u.s = args;
    return formatPacketCore(nodeStatus_fields, 3, u.data_arr, out_buffer);
}

static inline void sendnodeStatusFunction(sendnodeStatusArgs args) {
    if (++vitals_send_rate_controllers[4].counter < vitals_send_rate_controllers[4].divider) return;
    vitals_send_rate_controllers[4].counter = 0;
    uint8_t dataBuffer[2] = {0};
    args.mask = (int32_t)145; // Auto-assigned
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
    if (++vitals_send_rate_controllers[5].counter < vitals_send_rate_controllers[5].divider) return;
    vitals_send_rate_controllers[5].counter = 0;
    header.mask = (int32_t)1; // Auto-assigned
    union_sendunknownCanPacket u __attribute__((aligned(4)));
    u.s = header;
    sendPacketVariable(unknownCanPacket_fields, 6, u.data_arr, payload, payloadBytes);
}

// ----- CANDataFrame -----
typedef struct __attribute__((packed)) sendCANDataFrameHeader{
    int32_t mask;
    int32_t nodeID;
    int32_t frameID;
} sendCANDataFrameHeader;

typedef union {
    sendCANDataFrameHeader s;
    int32_t data_arr[3];
} union_sendCANDataFrame;

extern const simpleDataPoint CANDataFrame_fields[3];
static inline uint8_t formatCANDataFrameFunction(sendCANDataFrameHeader header, const uint8_t* payload, size_t payloadBytes, uint8_t* out_buffer) {
    header.mask = (int32_t)0; // Auto-assigned
    union_sendCANDataFrame u __attribute__((aligned(4)));
    u.s = header;
    return formatPacketVariable(CANDataFrame_fields, 3, u.data_arr, payload, payloadBytes, out_buffer);
}

static inline void sendCANDataFrameFunction(sendCANDataFrameHeader header, const uint8_t* payload, size_t payloadBytes) {
    if (++vitals_send_rate_controllers[6].counter < vitals_send_rate_controllers[6].divider) return;
    vitals_send_rate_controllers[6].counter = 0;
    header.mask = (int32_t)0; // Auto-assigned
    union_sendCANDataFrame u __attribute__((aligned(4)));
    u.s = header;
    sendPacketVariable(CANDataFrame_fields, 3, u.data_arr, payload, payloadBytes);
}


#ifdef __cplusplus
}
#endif

#endif // VITALS_PACKET_SEND_LUT_H
