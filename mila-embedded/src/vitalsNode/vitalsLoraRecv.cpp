
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "vitalsLoraRecv.hpp"

#include "../LoraCommon/LoraProtocol.h"
#include "../LoraCommon/blastProtocolConfig.hpp"
#include "vitalsRecvData.h"
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
    for(;;){
        if(protocolRecv(&packet)) {
            //print some info about the packet
            memcpy(&protocolPacket, packet.data, RXHeaderSize);
            ESP_LOGI(TAG, "Received packet with protocolID: %" PRIu16 ", flags: %" PRIu8 ", bitmap: %" PRIu16 ", dataSize: %zu, RSSI: %.2f, SNR: %.2f",
                     protocolPacket.protocolID, protocolPacket.flags, protocolPacket.bitmap, packet.dataSize, packet.RSSI, packet.SNR);

            //parse the packet
            processReceivedData(packet.data + RXHeaderSize, packet.dataSize - RXHeaderSize);
        } else {
            ESP_LOGW(TAG, "No packets received (Timeout)");
            vTaskDelay(pdMS_TO_TICKS(2000)); //wait a bit before trying again to avoid spamming logs, esp if driver is trying to reboot
        }
    }
    xTaskCreateStatic(loraMonitorTask, "Lora_Monitor_Task", LORA_TASK_SIZES, NULL, 1, LORA_Monitor_Stack, &LORA_Monitor_Buffer);

}
