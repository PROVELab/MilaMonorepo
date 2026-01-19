#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include "LoraTransmitAPI.hpp"

static const char* TAG= "main";

#define LORA_TASK_SIZES 4096
StaticTask_t LORA_Send_Buffer;
StackType_t LORA_Send_Stack[LORA_TASK_SIZES];

StaticTask_t LORA_Monitor_Buffer;
StackType_t LORA_Monitor_Stack[LORA_TASK_SIZES];

void loraSendTask(void* pvParameters){
    ESP_LOGI(TAG, "Lora Send Task started");
    uint64_t counter = 0;
    for(;;){
        ESP_LOGI(TAG, "main.cpp sending num %llu", counter);
        protocolTransmitOversizeFrame(0, (uint8_t*) (&counter), sizeof(counter));

        vTaskDelay(pdMS_TO_TICKS(2000)); //send every 2 seconds
        counter++;
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
  vTaskDelay(pdMS_TO_TICKS(3000)); //wait 5 seconds before starting TX
  ESP_LOGI(TAG, "Starting Lora TX from main");
  Lora_TX_Init(getStandardConfig(BoardType::Wio_SX1262, TestMode::lowPower));

  xTaskCreateStatic(loraSendTask, "Lora_Send_Task", LORA_TASK_SIZES, NULL, 1, LORA_Send_Stack, &LORA_Send_Buffer);
  xTaskCreateStatic(loraMonitorTask, "Lora_Monitor_Task", LORA_TASK_SIZES, NULL, 1, LORA_Monitor_Stack, &LORA_Monitor_Buffer); 

}

