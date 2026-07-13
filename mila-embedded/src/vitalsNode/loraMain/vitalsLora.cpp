
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "vitalsLora.h"

#include "../../LoraCommon/LoraProtocol.h"
#include "../../LoraCommon/blastProtocolConfig.hpp"
#include "../vitalsGen/vitalsPacketRecvLUT.h"
#include <cstring>

#include "vitalsLora.h"
#include "../loraStaticHelper/staticHelp.h"

static const char* TAG= "vitalsLora";

#define LORA_TASK_SIZES 4096

StaticTask_t LORA_Monitor_Buffer;
StackType_t LORA_Monitor_Stack[LORA_TASK_SIZES];
void loraMonitorTask(void* pvParameters);

StaticTask_t LORA_Read_Buffer;
StackType_t LORA_Read_Stack[LORA_TASK_SIZES];
void loraRecvTask(void* pvParameters);


extern "C" void vitalsLoraInit(const UBaseType_t lora_monitor_priority, const UBaseType_t lora_recv_priority){
    xTaskCreateStatic( loraMonitorTask, 
        "Lora_Monitor_Task", 
        LORA_TASK_SIZES, 
        NULL, 
        lora_monitor_priority,
        LORA_Monitor_Stack, &LORA_Monitor_Buffer);

    while(!LoraDriverRunning()) {
        vTaskDelay(pdMS_TO_TICKS(500));
        ESP_LOGI(TAG, "Waiting for Lora Driver to start. blocking here");
    }

    xTaskCreateStatic( loraRecvTask, 
        "Lora_Read_Task", 
        LORA_TASK_SIZES, 
        NULL, 
        lora_monitor_priority, 
        LORA_Read_Stack, 
        &LORA_Read_Buffer
    );

}

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
    for(;;){
        if(protocolRecv(&packet)) {
            if (packet.dataSize < RXHeaderSize) {
                ESP_LOGW(TAG, "Received undersized LoRa packet: %zu bytes", packet.dataSize);
                continue;
            }
            //print some info about the packet
            std::memcpy(&protocolPacket, packet.data, RXHeaderSize);
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
                    uint32_t received_mask = 0;
                    for (int shift = 0; shift < entry->mask_bits; shift += 8) {
                        received_mask |= ((uint32_t)payload_ptr[shift / 8]) << shift;
                    }
                    const uint32_t mask = (entry->mask_bits == 32) ? 0xFFFFFFFFu : ((1u << entry->mask_bits) - 1u);
                    received_mask &= mask;

                    if (received_mask == entry->mask_val) {
                        handler_found = true;
                        ESP_LOGI(TAG, "Lora RX: Matched handler for command with mask 0x%x (%d bits)", (unsigned int)entry->mask_val, entry->mask_bits);

                        int32_t bit_index = entry->mask_bits;
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
        } 
    }
}
