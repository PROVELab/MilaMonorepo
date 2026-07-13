#ifndef VITALS_PACKET_RECV_LUT_H
#define VITALS_PACKET_RECV_LUT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "pecan/pecan.h"
#include <stddef.h>
#include <stdint.h>

#define MAX_RECV_DATA_FIELDS 3
#define RECV_PACKET_TYPE_FIXED 0
#define RECV_PACKET_TYPE_CUSTOM 1

// ----- telem_to_vitals -----
typedef union {
    int32_t i32;
    telem_to_vitals_Commands e;
} union_telem_to_vitals_telem_to_vitals_Commands;

typedef struct __attribute__((packed)) {
    union_telem_to_vitals_telem_to_vitals_Commands telem_to_vitals_Commands;
} telem_to_vitals_args_t;

void ontelem_to_vitals(telem_to_vitals_args_t args);

// ----- set_telem_update_frequency -----
typedef struct __attribute__((packed)) {
    int32_t nodeID;
    int32_t packet_or_frame_ID;
    int32_t divider;
} set_telem_update_frequency_args_t;

void onset_telem_update_frequency(set_telem_update_frequency_args_t args);

// ----- setChargeCondition -----
typedef struct __attribute__((packed)) {
    int32_t min_MC_Voltage;
    int32_t minPercentCharged;
} setChargeCondition_args_t;

void onsetChargeCondition(setChargeCondition_args_t args);

// ----- setCoolantDutyCycle -----
typedef struct __attribute__((packed)) {
    int32_t dutyCycle;
} setCoolantDutyCycle_args_t;

void onsetCoolantDutyCycle(setCoolantDutyCycle_args_t args);

// ----- setCoolantFrequency_HZ -----
typedef struct __attribute__((packed)) {
    int32_t frequency_HZ;
} setCoolantFrequency_HZ_args_t;

void onsetCoolantFrequency_HZ(setCoolantFrequency_HZ_args_t args);

// ----- forward_packet -----
typedef struct __attribute__((packed)) {
    int32_t CAN_ID;
    int32_t dataLength;
    int32_t extendedID;
    const uint8_t* payload;
    size_t max_payload_size;
} forward_packet_args_t;

size_t onforward_packet(forward_packet_args_t args);


#ifdef __cplusplus
}
#endif

#endif // VITALS_PACKET_RECV_LUT_H
