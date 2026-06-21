#include "vitalsPacketRecvLUT.h"
#include "../loraStaticHelper/staticHelp.h"
#include "../loraStaticHelper/staticHelp.h"
#include "pecan/pecan.h" // For sendPacket, combinedID, etc.
#include "../../programConstants.h"
#include "esp_log.h"
#include <string.h>

// ----- telem_to_vitals -----
const simpleDataPoint telem_to_vitals_fields[1] = {
    { .bits=2, .min=0, .max=3 },
};

// ----- set_telem_update_frequency -----
const simpleDataPoint set_telem_update_frequency_fields[3] = {
    { .bits=4, .min=0, .max=15 },
    { .bits=8, .min=0, .max=255 },
    { .bits=8, .min=0, .max=255 },
};

// ----- setChargeCondition -----
const simpleDataPoint setChargeCondition_fields[2] = {
    { .bits=8, .min=80, .max=335 },
    { .bits=5, .min=68, .max=99 },
};

// ----- setCoolantDutyCycle -----
const simpleDataPoint setCoolantDutyCycle_fields[1] = {
    { .bits=7, .min=0, .max=100 },
};

// ----- setCoolantFrequency_HZ -----
const simpleDataPoint setCoolantFrequency_HZ_fields[1] = {
    { .bits=16, .min=0, .max=65535 },
};

// ----- forward_packet -----
const simpleDataPoint forward_packet_fields[3] = {
    { .bits=11, .min=0, .max=2047 },
    { .bits=4, .min=0, .max=15 },
    { .bits=1, .min=0, .max=1 },
};

// Wrapper for telem_to_vitals
static size_t ontelem_to_vitals_wrapper(const uint8_t* raw_packet, size_t packet_len, int32_t* bitIndex) {
    union { telem_to_vitals_args_t s; int32_t data_arr[1]; } u __attribute__((aligned(4)));

    unpackDataStream(u.data_arr, 1, raw_packet, telem_to_vitals_fields, bitIndex);
    ontelem_to_vitals(u.s);
    return 0;
}

// Wrapper for set_telem_update_frequency
static size_t onset_telem_update_frequency_wrapper(const uint8_t* raw_packet, size_t packet_len, int32_t* bitIndex) {
    union { set_telem_update_frequency_args_t s; int32_t data_arr[3]; } u __attribute__((aligned(4)));

    unpackDataStream(u.data_arr, 3, raw_packet, set_telem_update_frequency_fields, bitIndex);
    onset_telem_update_frequency(u.s);
    return 0;
}

// Wrapper for setChargeCondition
static size_t onsetChargeCondition_wrapper(const uint8_t* raw_packet, size_t packet_len, int32_t* bitIndex) {
    union { setChargeCondition_args_t s; int32_t data_arr[2]; } u __attribute__((aligned(4)));

    unpackDataStream(u.data_arr, 2, raw_packet, setChargeCondition_fields, bitIndex);
    onsetChargeCondition(u.s);

    // This packet is also forwarded to the target node 'precharge'.
    forwardLoraToCAN(prechargeID, 0, 0, setChargeCondition_fields, 2, RECV_PACKET_TYPE_FIXED, u.data_arr, raw_packet, packet_len, bitIndex);
    return 0;
}

// Wrapper for setCoolantDutyCycle
static size_t onsetCoolantDutyCycle_wrapper(const uint8_t* raw_packet, size_t packet_len, int32_t* bitIndex) {
    union { setCoolantDutyCycle_args_t s; int32_t data_arr[1]; } u __attribute__((aligned(4)));

    unpackDataStream(u.data_arr, 1, raw_packet, setCoolantDutyCycle_fields, bitIndex);
    onsetCoolantDutyCycle(u.s);

    // This packet is also forwarded to the target node 'powerDistribution'.
    forwardLoraToCAN(powerDistributionID, 0, 1, setCoolantDutyCycle_fields, 1, RECV_PACKET_TYPE_FIXED, u.data_arr, raw_packet, packet_len, bitIndex);
    return 0;
}

// Wrapper for setCoolantFrequency_HZ
static size_t onsetCoolantFrequency_HZ_wrapper(const uint8_t* raw_packet, size_t packet_len, int32_t* bitIndex) {
    union { setCoolantFrequency_HZ_args_t s; int32_t data_arr[1]; } u __attribute__((aligned(4)));

    unpackDataStream(u.data_arr, 1, raw_packet, setCoolantFrequency_HZ_fields, bitIndex);
    onsetCoolantFrequency_HZ(u.s);

    // This packet is also forwarded to the target node 'powerDistribution'.
    forwardLoraToCAN(powerDistributionID, 1, 1, setCoolantFrequency_HZ_fields, 1, RECV_PACKET_TYPE_FIXED, u.data_arr, raw_packet, packet_len, bitIndex);
    return 0;
}

// Wrapper for forward_packet
static size_t onforward_packet_wrapper(const uint8_t* raw_packet, size_t packet_len, int32_t* bitIndex) {
    union { forward_packet_args_t s; int32_t data_arr[3]; } u __attribute__((aligned(4)));

    unpackDataStream(u.data_arr, 3, raw_packet, forward_packet_fields, bitIndex);
    size_t fixed_bytes = (*bitIndex + 7) / 8;
    if (packet_len > fixed_bytes) { u.s.payload = raw_packet + fixed_bytes; u.s.max_payload_size = packet_len - fixed_bytes; } else { u.s.payload = NULL; u.s.max_payload_size = 0; }
    size_t custom_bytes_consumed = onforward_packet(u.s);
    *bitIndex += custom_bytes_consumed * 8;
    return 0; // For payload-bearing packets, consumption is now reflected in bitIndex.
}

const uint8_t MAX_RECV_MASK_BITS = 8;
const RecvPacketLUTEntry recvPacketLUT[] = {
    { // setChargeCondition
        .fields = setChargeCondition_fields,
        .num_fields = 2,
        .mask_val = 0,
        .mask_bits = 3,
        .packet_type = RECV_PACKET_TYPE_FIXED,
        .callback_wrapper = onsetChargeCondition_wrapper,
    },
    { // setCoolantDutyCycle
        .fields = setCoolantDutyCycle_fields,
        .num_fields = 1,
        .mask_val = 4,
        .mask_bits = 4,
        .packet_type = RECV_PACKET_TYPE_FIXED,
        .callback_wrapper = onsetCoolantDutyCycle_wrapper,
    },
    { // setCoolantFrequency_HZ
        .fields = setCoolantFrequency_HZ_fields,
        .num_fields = 1,
        .mask_val = 12,
        .mask_bits = 4,
        .packet_type = RECV_PACKET_TYPE_FIXED,
        .callback_wrapper = onsetCoolantFrequency_HZ_wrapper,
    },
    { // set_telem_update_frequency
        .fields = set_telem_update_frequency_fields,
        .num_fields = 3,
        .mask_val = 2,
        .mask_bits = 4,
        .packet_type = RECV_PACKET_TYPE_FIXED,
        .callback_wrapper = onset_telem_update_frequency_wrapper,
    },
    { // telem_to_vitals
        .fields = telem_to_vitals_fields,
        .num_fields = 1,
        .mask_val = 10,
        .mask_bits = 6,
        .packet_type = RECV_PACKET_TYPE_FIXED,
        .callback_wrapper = ontelem_to_vitals_wrapper,
    },
    { // forward_packet
        .fields = forward_packet_fields,
        .num_fields = 3,
        .mask_val = 42,
        .mask_bits = 8,
        .packet_type = RECV_PACKET_TYPE_CUSTOM,
        .callback_wrapper = onforward_packet_wrapper,
    },
};

const size_t recvPacketLUTSize = sizeof(recvPacketLUT) / sizeof(RecvPacketLUTEntry);
