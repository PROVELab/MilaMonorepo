#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "esp_timer.h"

#include "../LoraCommon/LoraProtocol.h"
#include "../LoraCommon/blastProtocolConfig.hpp"
#include <cstring>
static const char* TAG= "main";

#define LORA_TASK_SIZES 4096

StaticTask_t LORA_Monitor_Buffer;
StackType_t LORA_Monitor_Stack[LORA_TASK_SIZES];

StaticTask_t LORA_Read_Buffer;
StackType_t LORA_Read_Stack[LORA_TASK_SIZES];

//how many ints in each packet that TX sends to us
#define ints_per_packet (maxTXDataSize / sizeof(uint64_t))

struct entry{
    uint64_t packetNumber;
    uint8_t packetDataCorrupt;
    uint8_t protocolError;
    uint8_t packetWrongSize;
    uint8_t packetRecvPercent;
};

entry csvEntry;

driverRecvPacket packet;

//logs field in log, and relevant fields from driverPacket packet
inline void logEntry(){ // Renamed function back to logEntry
    uint64_t timeStamp_ms = esp_timer_get_time() / 1000;
    printf("csv entry: %" PRIu64 ",%" PRIu64 ",%" PRIu8 ",%" PRIu8 ",%" PRIu8 ",%f,%f,%d,%" PRIu8 "\n",
        timeStamp_ms, csvEntry.packetNumber, csvEntry.packetDataCorrupt, csvEntry.protocolError, csvEntry.packetWrongSize, 
        packet.RSSI, packet.SNR, packet.irqFlags, csvEntry.packetRecvPercent);
}

//for benchmarking, gives outputs for a csv identified by "csv entry"
void loraReadTask(void* pvParameters){
    ESP_LOGI(TAG, "Lora Read Task started");
    for(;;){
        // protocolRecv will wait up to 10 seconds for a packet.
        if(protocolRecv(&packet)) {
            csvEntry.packetNumber = 0;
            csvEntry.packetDataCorrupt = 0;
            csvEntry.protocolError = 0;
            csvEntry.packetWrongSize = 0;
            csvEntry.packetRecvPercent = 0;

            printRecvStatus(&packet);

            // To safely access the packet data without violating strict-aliasing rules,
            // we copy the data into a local, aligned struct.
            TXProtocolPacket txPacket;
            std::memcpy(&txPacket, packet.data, packet.dataSize);

            if(txPacket.flags & txFlagMasks::priorityPacketMask) {
                ESP_LOGW(TAG, "Received a priority packet!, this should contain an error");
                // Extract count from upper 4 bits of the first data byte
                uint8_t errorCount = (txPacket.data[0] >> 4) & 0x0F;
                for(uint8_t i = 0; i < errorCount; i++) {
                    int16_t errorCode;
                    // Safely copy each error code from the payload.
                    std::memcpy(&errorCode, txPacket.data + 1 + (i * sizeof(int16_t)), sizeof(int16_t));
                    ESP_LOGE(TAG, "Error from TX side %d: code %d", i, errorCode);
                }
            }

            // The total packet size should be the header size plus the user data size.
            if(packet.dataSize != (TXHeaderSize + (ints_per_packet * sizeof(uint64_t)))) {
                csvEntry.packetWrongSize = 1;
            } else {
                // Safely read the first integer to get the expected packet number.
                std::memcpy(&csvEntry.packetNumber, txPacket.data, sizeof(uint64_t));
                for(int i = 1; i < ints_per_packet; i++){
                    uint64_t current_int;
                    std::memcpy(&current_int, txPacket.data + (i * sizeof(uint64_t)), sizeof(uint64_t));
                    if(csvEntry.packetNumber != current_int){
                        csvEntry.packetDataCorrupt = 1;
                        break;
                    }
                }
            }

            uint16_t bitmap;
            uint8_t burstSize;
            getBitmap(&bitmap, &burstSize);
            if (burstSize > 0) {
                //log bitmap and burstSize
                char buf[17];
                for (int i = 0; i < 16; i++) {
                    buf[i] = (bitmap & (1 << (15 - i))) ? '1' : '0';
                }
                buf[16] = '\0';
                ESP_LOGI(TAG, "bitmap %s burstSize %"  PRIu8, buf, burstSize);
                //

                csvEntry.packetRecvPercent = (uint8_t)(100 * __builtin_popcount(bitmap) / burstSize);
            }

            logEntry(); // Call the renamed function
        } else {
            ESP_LOGW(TAG, "10 seconds of no new packets...");
        }
    }
}

void loraMonitorTask(void* pvParameters){
    ESP_LOGI(TAG, "Lora Monitor Task started");
    RadioConfig cfg = getStandardConfig(BoardType::Ebyte_SX1262, TestMode::lowPower);
    for(;;){
        //monitor for crashes
        char* errMsg = NULL;
        int16_t errCode = runProtocol(&cfg, &errMsg);
        ESP_LOGE(TAG, "Lora Driver crashed with code %d, msg: %s", errCode, errMsg);
        vTaskDelay(pdMS_TO_TICKS(1000)); //wait a sec before retrying
    }
}

extern "C" void app_main(void) {
    vTaskDelay(pdMS_TO_TICKS(5000));

  // loop forever
    xTaskCreateStatic(loraMonitorTask, "Lora_Monitor_Task", LORA_TASK_SIZES, NULL, 1, LORA_Monitor_Stack, &LORA_Monitor_Buffer);

    xTaskCreateStatic(loraReadTask, "Lora_Read_Task", LORA_TASK_SIZES, NULL, 1, LORA_Read_Stack, &LORA_Read_Buffer);

}
