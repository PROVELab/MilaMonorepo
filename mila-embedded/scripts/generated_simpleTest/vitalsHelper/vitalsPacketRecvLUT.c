#include "vitalsPacketRecvLUT.h"
#include "pecan/pecan.h" // For sendPacket, combinedID, etc.
#include "esp_log.h"
#include <string.h>

static const char* TAG = "VitalsRecvLUT";
#define TELEMETRY_COMMAND_FC 0x0A // Function code for forwarded telemetry commands

// ----- genericVitalsCommand -----
const simpleDataPoint genericVitalsCommand_fields[1] = {
    { .bits=1, .min=0, .max=0 },
};

// ----- set_telem_update_frequency_divider -----
const simpleDataPoint set_telem_update_frequency_divider_fields[1] = {
    { .bits=4, .min=0, .max=15 },
};

// ----- prechargeCommand -----
const simpleDataPoint prechargeCommand_fields[1] = {
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
    genericVitalsCommand_args_t args;
    int32_t* dest_ptr = (int32_t*)&args;
    for (int i = 0; i < 1; ++i) {
        pecan_unpack(&dest_ptr[i], raw_packet, &genericVitalsCommand_fields[i], bitIndex);
    }
    ongenericVitalsCommand(args);
    return 0; // FIXED packets don't consume payload
}

// Wrapper for set_telem_update_frequency_divider
static size_t onset_telem_update_frequency_divider_wrapper(const uint8_t* raw_packet, size_t packet_len, int8_t* bitIndex) {
    set_telem_update_frequency_divider_args_t args;
    int32_t* dest_ptr = (int32_t*)&args;
    for (int i = 0; i < 1; ++i) {
        pecan_unpack(&dest_ptr[i], raw_packet, &set_telem_update_frequency_divider_fields[i], bitIndex);
    }
    onset_telem_update_frequency_divider(args);
    return 0; // FIXED packets don't consume payload
}

// Wrapper for prechargeCommand
static size_t onprechargeCommand_wrapper(const uint8_t* raw_packet, size_t packet_len, int8_t* bitIndex) {
    prechargeCommand_args_t args;
    int32_t* dest_ptr = (int32_t*)&args;
    for (int i = 0; i < 1; ++i) {
        pecan_unpack(&dest_ptr[i], raw_packet, &prechargeCommand_fields[i], bitIndex);
    }
    onprechargeCommand(args);
    return 0; // FIXED packets don't consume payload

    // This packet is forwarded to the target node 'prechargeID'.
    {
        CANPacket can_p;
        memset(&can_p, 0, sizeof(CANPacket));
        can_p.id = combinedID(TELEMETRY_COMMAND_FC, 3);
        int8_t can_bit_idx = 0;

        // Re-pack the data fields for the CAN packet
        const int32_t* src_ptr = (const int32_t*)&args;
        for (int i = 0; i < 1; ++i) {
            const simpleDataPoint* field_info = &prechargeCommand_fields[i];
            uint32_t formatted_val;
            pecan_pack(&formatted_val, src_ptr[i], field_info);
            copyValueToData(&formatted_val, can_p.data, &can_bit_idx, field_info->bits);
            can_bit_idx += field_info->bits;
        }

        can_p.dataSize = (can_bit_idx + 7) / 8;
        sendPacket(&can_p);
    }
}

// Wrapper for forward_packet
static size_t onforward_packet_wrapper(const uint8_t* raw_packet, size_t packet_len, int8_t* bitIndex) {
    forward_packet_args_t args;
    int32_t* dest_ptr = (int32_t*)&args;
    for (int i = 0; i < 3; ++i) {
        pecan_unpack(&dest_ptr[i], raw_packet, &forward_packet_fields[i], bitIndex);
    }
    size_t fixed_bytes = (*bitIndex + 7) / 8;
    if (packet_len > fixed_bytes) {
        args.payload = raw_packet + fixed_bytes;
        args.max_payload_size = packet_len - fixed_bytes;
    } else {
        args.payload = NULL;
        args.max_payload_size = 0;
    }
    return onforward_packet(args);
}

const uint8_t MAX_RECV_MASK_BITS = 7;
const RecvPacketLUTEntry recvPacketLUT[] = {
    { // forward_packet
        .fields = forward_packet_fields,
        .num_fields = 3,
        .mask_val = 0,
        .mask_bits = 0,
        .packet_type = RECV_PACKET_TYPE_CUSTOM,
        .callback_wrapper = onforward_packet_wrapper,
    },
    { // set_telem_update_frequency_divider
        .fields = set_telem_update_frequency_divider_fields,
        .num_fields = 1,
        .mask_val = 0,
        .mask_bits = 4,
        .packet_type = RECV_PACKET_TYPE_FIXED,
        .callback_wrapper = onset_telem_update_frequency_divider_wrapper,
    },
    { // prechargeCommand
        .fields = prechargeCommand_fields,
        .num_fields = 1,
        .mask_val = 0,
        .mask_bits = 6,
        .packet_type = RECV_PACKET_TYPE_FIXED,
        .callback_wrapper = onprechargeCommand_wrapper,
    },
    { // genericVitalsCommand
        .fields = genericVitalsCommand_fields,
        .num_fields = 1,
        .mask_val = 0,
        .mask_bits = 7,
        .packet_type = RECV_PACKET_TYPE_FIXED,
        .callback_wrapper = ongenericVitalsCommand_wrapper,
    },
};

const size_t recvPacketLUTSize = sizeof(recvPacketLUT) / sizeof(RecvPacketLUTEntry);
