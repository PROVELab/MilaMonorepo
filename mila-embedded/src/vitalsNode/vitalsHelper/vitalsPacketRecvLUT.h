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

// ----- genericVitalsCommand -----
typedef union {
    int32_t i32;
    vitalsCommands e;
} union_genericVitalsCommand_vitalsCommands;

typedef struct __attribute__((packed)) {
    union_genericVitalsCommand_vitalsCommands vitalsCommands;
} genericVitalsCommand_args_t;

void ongenericVitalsCommand(genericVitalsCommand_args_t args);

// ----- set_telem_update_frequency_divider -----
typedef struct __attribute__((packed)) {
    int32_t divider;
} set_telem_update_frequency_divider_args_t;

void onset_telem_update_frequency_divider(set_telem_update_frequency_divider_args_t args);

// ----- prechargeCommand -----
typedef union {
    int32_t i32;
    prechargeCommands e;
} union_prechargeCommand_prechargeCommands;

typedef struct __attribute__((packed)) {
    union_prechargeCommand_prechargeCommands prechargeCommands;
} prechargeCommand_args_t;

void onprechargeCommand(prechargeCommand_args_t args);

// ----- prechargeValue -----
typedef struct __attribute__((packed)) {
    int32_t value1;
} prechargeValue_args_t;

void onprechargeValue(prechargeValue_args_t args);

// ----- forward_packet -----
typedef struct __attribute__((packed)) {
    int32_t CAN_ID;
    int32_t dataLength;
    int32_t extendedID;
    const uint8_t* payload;
    size_t max_payload_size;
} forward_packet_args_t;

size_t onforward_packet(forward_packet_args_t args);

typedef struct {
    const simpleDataPoint* fields;
    uint8_t num_fields;
    uint8_t packet_type; // RECV_PACKET_TYPE_FIXED or RECV_PACKET_TYPE_CUSTOM
    uint32_t mask_val;
    uint8_t mask_bits;
    size_t (*callback_wrapper)(const uint8_t* raw_packet, size_t packet_len, int8_t* initial_bitIndex);
} RecvPacketLUTEntry;

extern const uint8_t MAX_RECV_MASK_BITS;
extern const RecvPacketLUTEntry recvPacketLUT[];
extern const size_t recvPacketLUTSize;

#ifdef __cplusplus
}
#endif

#endif // VITALS_PACKET_RECV_LUT_H
