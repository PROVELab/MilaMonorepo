#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include "../LoraCommon/LoraProtocol.hpp"

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
        protocolTransmit((uint8_t*) (&counter), sizeof(counter));
        vTaskDelay(pdMS_TO_TICKS(2000)); //send every 2 seconds
        counter++;
    }
}

void loraMonitorTask(void* pvParameters){
    ESP_LOGI(TAG, "Lora Monitor Task started");
    RadioConfig cfg = getStandardConfig(BoardType::Wio_SX1262, TestMode::lowPower);
    for(;;){
        //monitor for crashes
        char* errMsg;
        int16_t errCode = runProtocol(&cfg, errMsg);
        ESP_LOGE(TAG, "Lora Driver crashed with code %d, msg: %s", errCode, errMsg);
        vTaskDelay(pdMS_TO_TICKS(1000)); //check every second
    }
}

extern "C" void app_main(void) {

    xTaskCreateStatic(loraSendTask, "Lora_Send_Task", LORA_TASK_SIZES, NULL, 1, LORA_Send_Stack, &LORA_Send_Buffer);
    xTaskCreateStatic(loraMonitorTask, "Lora_Monitor_Task", LORA_TASK_SIZES, NULL, 1, LORA_Monitor_Stack, &LORA_Monitor_Buffer);

}

