#include "vitalsHelper/vitalsPacketRecvLUT.h"
#include "esp_log.h"
#include "pecan/pecan.h"
#include "../programConstants.h"
#include "vitalsRecvData.h"
#include <string.h>

static const char* TAG = "VitalsRecvData";

/**
 * @brief Processes a single packet entry that has been matched from the LUT.
 *
 * @param entry             The matched LUT entry.
 * @param stream_ptr        A reference to the stream's current position pointer.
 * @param remaining_len     A reference to the stream's remaining length.
 * @return true if the packet was processed successfully, false on error.
 */
static bool processFoundPacket(const RecvPacketLUTEntry* entry, const uint8_t*& stream_ptr, size_t& remaining_len) {
    ESP_LOGD(TAG, "Found packet with mask: %lu (0x%lX), bits: %u", entry->mask_val, entry->mask_val, entry->mask_bits);

    int8_t bitIndex = entry->mask_bits;
    size_t payload_consumed_bytes = 0;
    
    if (entry->callback_wrapper) {
        payload_consumed_bytes = entry->callback_wrapper(stream_ptr, remaining_len, &bitIndex);
    } else {
        ESP_LOGW(TAG, "No callback wrapper defined for packet with mask: %lu (0x%lX)", entry->mask_val, entry->mask_val);
    }

    const size_t header_bytes = (bitIndex + 7) / 8;
    const size_t total_packet_bytes = header_bytes + payload_consumed_bytes;

    ESP_LOGD(TAG, "Packet consumed %zu bytes (header: %zu, payload: %zu)", total_packet_bytes, header_bytes, payload_consumed_bytes);

    if (total_packet_bytes == 0 || total_packet_bytes > remaining_len) {
        ESP_LOGE(TAG, "Packet handler reported invalid consumed length (%zu bytes). Stopping stream processing.", total_packet_bytes);
        return false;
    }

    stream_ptr += total_packet_bytes;
    remaining_len -= total_packet_bytes;
    return true;
}

/**
 * @brief Finds and processes the next packet in the data stream.
 *
 * @param stream_ptr        A reference to the stream's current position pointer.
 * @param remaining_len     A reference to the stream's remaining length.
 * @param stream_start      A pointer to the start of the original data stream for logging offsets.
 * @return true if a packet was successfully parsed, false if no match was found or an error occurred.
 */
static bool parseNextMessage(const uint8_t*& stream_ptr, size_t& remaining_len, const uint8_t* stream_start) {
    ESP_LOGD(TAG, "Parsing next message: %zu bytes remaining at offset %td.", remaining_len, stream_ptr - stream_start);

    // Find the longest prefix match in the LUT by iterating backwards.
    for (int i = recvPacketLUTSize - 1; i >= 0; --i) {
        const RecvPacketLUTEntry* entry = &recvPacketLUT[i];
        if (entry->mask_bits > 0 && entry->mask_bits <= remaining_len * 8) {
            const uint32_t current_mask = (*(const uint32_t*)stream_ptr) & ((1UL << entry->mask_bits) - 1);
            if (current_mask == entry->mask_val) {
                return processFoundPacket(entry, stream_ptr, remaining_len);
            }
        }
    }

    ESP_LOGE(TAG, "No matching packet mask found in stream. current byte: (0x%X). " \
        "Stopping processing. (is vitals and telem codeGen in sync?)", stream_ptr[0]);
    return false;
}

/**
 * @brief Processes a raw byte buffer received from telemetry.
 *
 * This function acts as the entry point for incoming data. It uses the
 * auto-generated LUTs to decode the packet and dispatch to the appropriate
 * callback.
 */
void processReceivedData(uint8_t* data, size_t len) {
    ESP_LOGI(TAG, "Processing %zu bytes of received data stream.", len);
    
    const uint8_t* stream_ptr = data;
    size_t remaining_len = len;

    while (remaining_len > 0) {
        if (!parseNextMessage(stream_ptr, remaining_len, data)) {
            //error with parsing somehow (maybe not aligned on header, or recv invalid header)
            return;
        }
    }
}

/**
 * @brief Forwards a telemetry command to another node on the CAN bus.
 * This function constructs a new CAN packet from the telemetry data and sends it.
 */
void forwardCANPacket(
    uint32_t targetNodeId,
    uint32_t can_mask_val,
    uint8_t can_mask_bits,
    const simpleDataPoint* fields,
    uint8_t num_fields,
    uint8_t packet_type,
    const void* args_ptr,
    const uint8_t* raw_telem_packet,
    size_t telem_packet_len,
    int8_t* telem_bit_idx_ptr)
{
    CANPacket can_p;
    memset(&can_p, 0, sizeof(CANPacket));
    can_p.id = combinedID(TelemetryCommand, targetNodeId);
    int8_t can_bit_idx = 0;

    // 1. Pack the CAN-level mask for the target node
    if (can_mask_bits > 0) {
        uint32_t temp_mask_val = can_mask_val;
        copyValueToData(&temp_mask_val, can_p.data, can_bit_idx, can_mask_bits);
        can_bit_idx += can_mask_bits;
    }

    // 2. Re-pack the data fields for the CAN packet from the args struct
    if (num_fields > 0 && args_ptr != NULL) {
        const int32_t* src_ptr = (const int32_t*)args_ptr;
        for (int i = 0; i < num_fields; ++i) {
            const simpleDataPoint* field_info = &fields[i];
            uint32_t formatted_val;
            pecan_pack(&formatted_val, src_ptr[i], field_info);
            copyValueToData(&formatted_val, can_p.data, can_bit_idx, field_info->bits);
            can_bit_idx += field_info->bits;
        }
    }

    // 3. Handle CUSTOM payload if necessary
    if (packet_type == RECV_PACKET_TYPE_CUSTOM) {
        // The incoming telemetry packet's header is guaranteed to be byte-aligned for CUSTOM packets.
        size_t telem_header_bytes = *telem_bit_idx_ptr / 8;
        size_t payload_len = (telem_packet_len > telem_header_bytes) ? (telem_packet_len - telem_header_bytes) : 0;
        // The new CAN packet header is NOT guaranteed to be byte-aligned, so we must round up.
        size_t can_header_bytes = (can_bit_idx + 7) / 8;
        if (payload_len > 0 && (can_header_bytes + payload_len <= MAX_SIZE_PACKET_DATA)) {
            memcpy(can_p.data + can_header_bytes, raw_telem_packet + telem_header_bytes, payload_len);
            can_p.dataSize = can_header_bytes + payload_len;
        } else {
            can_p.dataSize = can_header_bytes;
        }
    } else {
        // FIXED packets are not guaranteed to be byte-aligned.
        can_p.dataSize = (can_bit_idx + 7) / 8;
    }

    sendPacket(&can_p);
}