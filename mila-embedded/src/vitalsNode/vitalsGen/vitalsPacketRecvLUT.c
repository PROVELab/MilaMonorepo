#include "vitalsPacketRecvLUT.h"
#include "../vitalsLoraRecv.hpp"
#include "pecan/pecan.h" // For sendPacket, combinedID, etc.
#include "../../programConstants.h"
#include "esp_log.h"
#include <string.h>

// ----- genericVitalsCommand -----
const simpleDataPoint genericVitalsCommand_fields[1] = {
    { .bits=1, .min=0, .max=1 },
};

// ----- set_telem_update_frequency -----
const simpleDataPoint set_telem_update_frequency_fields[3] = {
    { .bits=4, .min=0, .max=15 },
    { .bits=8, .min=0, .max=255 },
    { .bits=8, .min=0, .max=255 },
};

// ----- prechargeCommand -----
const simpleDataPoint prechargeCommand_fields[1] = {
    { .bits=2, .min=0, .max=2 },
};

// ----- intermoduleCommand -----
const simpleDataPoint intermoduleCommand_fields[1] = {
    { .bits=2, .min=0, .max=2 },
};

// ----- forward_packet -----
const simpleDataPoint forward_packet_fields[3] = {
    { .bits=11, .min=0, .max=2047 },
    { .bits=4, .min=0, .max=15 },
    { .bits=1, .min=0, .max=1 },
};

// Wrapper for genericVitalsCommand
static size_t ongenericVitalsCommand_wrapper(const uint8_t* raw_packet, size_t packet_len, int8_t* bitIndex) {
    union { genericVitalsCommand_args_t s; int32_t data_arr[1]; } u __attribute__((aligned(4)));

    for (int i = 0; i < 1; ++i) {
        pecan_unpack(&u.data_arr[i], raw_packet, &genericVitalsCommand_fields[i], bitIndex);
    }
    ongenericVitalsCommand(u.s);
    return 0;
}

// Wrapper for set_telem_update_frequency
static size_t onset_telem_update_frequency_wrapper(const uint8_t* raw_packet, size_t packet_len, int8_t* bitIndex) {
    union { set_telem_update_frequency_args_t s; int32_t data_arr[3]; } u __attribute__((aligned(4)));

    for (int i = 0; i < 3; ++i) {
        pecan_unpack(&u.data_arr[i], raw_packet, &set_telem_update_frequency_fields[i], bitIndex);
    }
    onset_telem_update_frequency(u.s);
    return 0;
}

// Wrapper for prechargeCommand
static size_t onprechargeCommand_wrapper(const uint8_t* raw_packet, size_t packet_len, int8_t* bitIndex) {

    // This packet is forwarded to the target node 'prechargeID'.
    int32_t data_arr[1];
    for (int i = 0; i < 1; ++i) {
        pecan_unpack(&data_arr[i], raw_packet, &prechargeCommand_fields[i], bitIndex);
    }
    forwardCANPacket(0, 0, 0, prechargeCommand_fields, 1, RECV_PACKET_TYPE_FIXED, data_arr, raw_packet, packet_len, bitIndex);
    return 0; // Forwarded packets are not processed locally by vitals
}

// Wrapper for intermoduleCommand
static size_t onintermoduleCommand_wrapper(const uint8_t* raw_packet, size_t packet_len, int8_t* bitIndex) {

    // This packet is forwarded to the target node 'powerDistribution'.
    int32_t data_arr[1];
    for (int i = 0; i < 1; ++i) {
        pecan_unpack(&data_arr[i], raw_packet, &intermoduleCommand_fields[i], bitIndex);
    }
    forwardCANPacket(0, 0, 0, intermoduleCommand_fields, 1, RECV_PACKET_TYPE_FIXED, data_arr, raw_packet, packet_len, bitIndex);
    return 0; // Forwarded packets are not processed locally by vitals
}

// Wrapper for forward_packet
static size_t onforward_packet_wrapper(const uint8_t* raw_packet, size_t packet_len, int8_t* bitIndex) {
    union { forward_packet_args_t s; int32_t data_arr[3]; } u __attribute__((aligned(4)));

    for (int i = 0; i < 3; ++i) {
        pecan_unpack(&u.data_arr[i], raw_packet, &forward_packet_fields[i], bitIndex);
    }
    size_t fixed_bytes = (*bitIndex + 7) / 8;
    if (packet_len > fixed_bytes) { u.s.payload = raw_packet + fixed_bytes; u.s.max_payload_size = packet_len - fixed_bytes; } else { u.s.payload = NULL; u.s.max_payload_size = 0; }
    size_t custom_bytes_consumed = onforward_packet(u.s);
    *bitIndex += custom_bytes_consumed * 8;
    return 0; // For CUSTOM packets, consumption is now reflected in bitIndex.
}

const uint8_t MAX_RECV_MASK_BITS = 8;
const RecvPacketLUTEntry recvPacketLUT[] = {
    { // set_telem_update_frequency
        .fields = set_telem_update_frequency_fields,
        .num_fields = 3,
        .mask_val = 0,
        .mask_bits = 4,
        .packet_type = RECV_PACKET_TYPE_FIXED,
        .callback_wrapper = onset_telem_update_frequency_wrapper,
    },
    { // intermoduleCommand
        .fields = intermoduleCommand_fields,
        .num_fields = 1,
        .mask_val = 8,
        .mask_bits = 6,
        .packet_type = RECV_PACKET_TYPE_FIXED,
        .callback_wrapper = onintermoduleCommand_wrapper,
    },
    { // prechargeCommand
        .fields = prechargeCommand_fields,
        .num_fields = 1,
        .mask_val = 40,
        .mask_bits = 6,
        .packet_type = RECV_PACKET_TYPE_FIXED,
        .callback_wrapper = onprechargeCommand_wrapper,
    },
    { // genericVitalsCommand
        .fields = genericVitalsCommand_fields,
        .num_fields = 1,
        .mask_val = 24,
        .mask_bits = 7,
        .packet_type = RECV_PACKET_TYPE_FIXED,
        .callback_wrapper = ongenericVitalsCommand_wrapper,
    },
    { // forward_packet
        .fields = forward_packet_fields,
        .num_fields = 3,
        .mask_val = 88,
        .mask_bits = 8,
        .packet_type = RECV_PACKET_TYPE_CUSTOM,
        .callback_wrapper = onforward_packet_wrapper,
    },
};

const size_t recvPacketLUTSize = sizeof(recvPacketLUT) / sizeof(RecvPacketLUTEntry);
