
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "vitalsLoraRecv.hpp"

#include "../LoraCommon/LoraProtocol.h"
#include "../LoraCommon/blastProtocolConfig.hpp"
#include "vitalsGen/vitalsPacketRecvLUT.h"

#include <inttypes.h>
#include <string.h>

static const char* TAG= "main";

#define LORA_TASK_SIZES 4096

StaticTask_t LORA_Monitor_Buffer;
StackType_t LORA_Monitor_Stack[LORA_TASK_SIZES];

void loraMonitorTask(void* pvParameters){
    ESP_LOGI(TAG, "Lora Monitor Task started");
    RadioConfig cfg = getStandardConfig(Ebyte_SX1262, lowPower);
    for(;;){
        //monitor for crashes
        char* errMsg = NULL;
        int16_t errCode = runProtocol(&cfg, &errMsg);
        ESP_LOGE(TAG, "Lora Driver crashed with code %d, msg: %s", errCode, errMsg);
        vTaskDelay(pdMS_TO_TICKS(1000)); //check every second
    }
}

static driverRecvPacket packet;
static RXProtocolPacket protocolPacket;      //using protocolPacket to get alignment of header.

void loraRecvTask(void* pvParameters){
    
    ESP_LOGI(TAG, "Lora Read Task started");
    xTaskCreateStatic(loraMonitorTask, "Lora_Monitor_Task", LORA_TASK_SIZES, NULL, 1, LORA_Monitor_Stack, &LORA_Monitor_Buffer);

    for(;;){
        if(protocolRecv(&packet)) {
            //print some info about the packet
            memcpy(&protocolPacket, packet.data, RXHeaderSize);
            ESP_LOGI(TAG, "Received packet with protocolID: %" PRIu16 ", flags: %" PRIu8 ", bitmap: %" PRIu16 ", dataSize: %zu, RSSI: %.2f, SNR: %.2f",
                     protocolPacket.protocolID, protocolPacket.flags, protocolPacket.bitmap, packet.dataSize, packet.RSSI, packet.SNR);
            
            const uint8_t* payload_ptr = packet.data + RXHeaderSize;
            size_t bytes_remaining = packet.dataSize - RXHeaderSize;

            // A single Lora payload can contain multiple, concatenated, byte-aligned commands.
            // Loop through the payload until all bytes are processed.
            while (bytes_remaining > 0) {
                bool handler_found = false;

                // The inner loop finds the handler for the command at the current payload position.
                // The LUT is sorted by mask_bits, allowing for efficient, unambiguous decoding.
                for (size_t i = 0; i < recvPacketLUTSize; ++i) {
                    const RecvPacketLUTEntry* entry = &recvPacketLUT[i];

                    // Ensure the remaining payload is large enough for the mask we're checking.
                    if ((entry->mask_bits + 7) / 8 > bytes_remaining) {
                        continue;
                    }

                    // Extract the mask from the beginning of the current payload chunk.
                    // Since each command is byte-aligned, the bit offset is 0 relative to payload_ptr.
                    uint32_t received_mask = 0;
                    copyDataToValue(&received_mask, payload_ptr, 0, entry->mask_bits);

                    if (received_mask == entry->mask_val) {
                        handler_found = true;
                        ESP_LOGI(TAG, "Lora RX: Matched handler for command with mask 0x%x (%d bits)", (unsigned int)entry->mask_val, entry->mask_bits);

                        int8_t bit_index = entry->mask_bits;
                        if (entry->callback_wrapper) {
                            // The wrapper function unpacks all data and updates the bit_index
                            // to point to the end of the current command in the payload.
                            entry->callback_wrapper(payload_ptr, bytes_remaining, &bit_index);
                        }

                        // The final bit_index holds the total bits of the entire command (mask + fields + custom payload).
                        // The code generator ensures this is byte-aligned.
                        size_t bytes_consumed_by_this_packet = (bit_index + 7) / 8;

                        if (bytes_consumed_by_this_packet > ((entry->mask_bits + 7) / 8) && bytes_consumed_by_this_packet <= bytes_remaining) {
                            payload_ptr += bytes_consumed_by_this_packet;
                            bytes_remaining -= bytes_consumed_by_this_packet;
                        } else {
                            ESP_LOGE(TAG, "Packet handler reported consuming %zu bytes, but only %zu remain. Stopping parse.", bytes_consumed_by_this_packet, bytes_remaining);
                            bytes_remaining = 0; // Stop processing to prevent errors.
                        }

                        break; // Exit inner for-loop (LUT scan) and process next command in payload.
                    }
                }

                if (!handler_found) {
                    ESP_LOGW(TAG, "No matching handler found at current payload position. Stopping parse.");
                    bytes_remaining = 0; // Stop processing.
                }
            }
        } else {
            ESP_LOGW(TAG, "No packets received (Timeout)");
            vTaskDelay(pdMS_TO_TICKS(2000)); //wait a bit before trying again to avoid spamming logs, esp if driver is trying to reboot
        }
    }

}

// called by the wrappers to forward their packets
void forwardCANPacket(uint32_t target_node_id, uint32_t can_mask, uint8_t can_mask_bits, const simpleDataPoint* fields, uint8_t num_fields, uint8_t packet_type, const int32_t* data_arr, const uint8_t* raw_packet, size_t packet_len, int8_t* bitIndex) {
    uint8_t can_payload[8] = {0};
    int8_t can_bit_index = 0;

    if (can_mask_bits > 0) {
        simpleDataPoint can_mask_field = { .min = 0, .max = static_cast<int32_t>((1U << can_mask_bits) - 1), .bits = can_mask_bits };
        pecan_pack(can_payload, &can_bit_index, can_mask, &can_mask_field);
    }

    for (int i = 0; i < num_fields; i++) {
        pecan_pack(can_payload, &can_bit_index, data_arr[i], &fields[i]);
    }

    CANPacket p;
    memset(&p, 0, sizeof(p));
    p.id = combinedID(TelemetryCommand, target_node_id);
    size_t bytes_to_write = (can_bit_index + 7) / 8;
    if (bytes_to_write > 8) { ESP_LOGE(TAG, "Forwarded packet too large!"); return; }
    writeData(&p, (int8_t*)can_payload, bytes_to_write);
    sendPacket(&p);
}
