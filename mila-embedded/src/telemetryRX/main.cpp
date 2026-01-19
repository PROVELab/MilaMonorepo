#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include "LoraRecvAPI.hpp"
static const char* TAG= "main";

#define LORA_TASK_SIZES 4096

StaticTask_t LORA_Monitor_Buffer;
StackType_t LORA_Monitor_Stack[LORA_TASK_SIZES];

void loraReadTask(void* pvParameters){
    ESP_LOGI(TAG, "Lora Monitor Task started");
    uint8_t* rxBuffer;
    size_t loraPacketLen;
    for(;;){
        //monitor for crashes
        loraPacketLen =LoraAPIRead(rxBuffer);
        //for now just print the packet
        ESP_LOGI(TAG, "Received LoRa Packet - Length: %u", loraPacketLen);
        printf("Payload (Hex): ");
        for (size_t i = 0; i < loraPacketLen; i++) {
            printf("%02X ", rxBuffer[i]);
        }
        printf("\n");
        vTaskDelay(pdMS_TO_TICKS(1000)); //check every second
    }
}

void loraMonitorTask(void* pvParameters){
    ESP_LOGI(TAG, "Lora Monitor Task started");
    for(;;){
        //monitor for crashes
        char* errMsg;
        uint8_t msgLen;
        int16_t errCode = Lora_Monitor_Crash(errMsg, msgLen);
        ESP_LOGE(TAG, "Lora Driver crashed with code %d, msg: %.*s", errCode, msgLen, errMsg);
        vTaskDelay(pdMS_TO_TICKS(1000)); //check every second
        Lora_TX_Restart(getStandardConfig(BoardType::Wio_SX1262, TestMode::lowPower));
    }
}

extern "C" void app_main(void) {

  // loop forever
  LoraDriverInit();
  LoraStartRecv();
  xTaskCreateStatic(loraMonitorTask, "Lora_Monitor_Task", LORA_TASK_SIZES, NULL, 1, LORA_Monitor_Stack, &LORA_Monitor_Buffer);
}

