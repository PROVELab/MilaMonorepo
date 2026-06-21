#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "staticHelp.h"

#include "../../LoraCommon/LoraProtocol.h"
#include "../../LoraCommon/blastProtocolConfig.hpp"
#include "../vitalsGen/vitalsPacketRecvLUT.h"

#include <inttypes.h>
#include <cstring>

static const char* TAG = "LoraToCAN";

// called by the wrappers to forward their packets
extern "C" void forwardLoraToCAN(uint32_t target_node_id, uint32_t can_mask, uint8_t can_mask_bits, const simpleDataPoint* fields, uint8_t num_fields, uint8_t packet_type, const int32_t* data_arr, const uint8_t* raw_packet, size_t packet_len, int32_t* bitIndex) {
    uint8_t can_payload[8] = {0};
    int8_t can_bit_index = 0;

    if (can_mask_bits > 0) {
        simpleDataPoint can_mask_field = { .min = 0, .max = static_cast<int32_t>((1U << can_mask_bits) - 1), .bits = can_mask_bits };
        pecan_pack(&can_payload, &can_bit_index, can_mask, &can_mask_field);
    }

    for (int i = 0; i < num_fields; i++) {
        pecan_pack(&can_payload, &can_bit_index, data_arr[i], &fields[i]);
    }

    size_t bytes_to_write = (can_bit_index + 7) / 8;
    if (packet_type == RECV_PACKET_TYPE_CUSTOM) {
        const size_t fixed_bytes = (*bitIndex + 7) / 8;
        const size_t payload_bytes = (packet_len > fixed_bytes) ? (packet_len - fixed_bytes) : 0;
        if (bytes_to_write + payload_bytes > sizeof(can_payload)) {
            ESP_LOGE(TAG, "Forwarded payload packet too large! header=%zu payload=%zu", bytes_to_write, payload_bytes);
            return;
        }
        if (payload_bytes > 0) {
            std::memcpy(can_payload + bytes_to_write, raw_packet + fixed_bytes, payload_bytes);
            bytes_to_write += payload_bytes;
        }
    }

    CANPacket p;
    std::memset(&p, 0, sizeof(p));
    p.id = combinedID(TelemetryCommand, target_node_id);
    if (bytes_to_write > 8) { ESP_LOGE(TAG, "Forwarded packet too large!"); return; }
    writeData(&p, (int8_t*)can_payload, bytes_to_write);
    sendPacket(&p);
}
