#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include "../LoraCommon/LoraProtocol.hpp"

static const char* TAG= "main";

#define LORA_TASK_SIZES 4096
StaticTask_t LORA_Send_Buffer;
StackType_t LORA_Send_Stack[LORA_TASK_SIZES];

StaticTask_t LORA_Monitor_Buffer;
StackType_t LORA_Monitor_Stack[LORA_TASK_SIZES];

uint8_t data[maxTXDataSize] = {0};

#define ints_per_packet (maxTXDataSize / sizeof(uint64_t))

void loraSendTask(void* pvParameters){
    ESP_LOGI(TAG, "Lora Send Task started");
    uint64_t packet_counter = 0;
    for(;;){
        uint64_t* int_data = (uint64_t*)data;
        for(int i = 0; i < ints_per_packet; i++){
            int_data[i] = packet_counter;
        }

        // Send 10 packets fast, 10 slow, may overflow a bit, and test inconsistent stream
        uint32_t delay_ms;
        delay_ms = ((packet_counter % 20) < 10) ? 50 : 400;

        ESP_LOGI(TAG, "main.cpp sending num %llu", packet_counter);
        while(protocolTransmit(data, ints_per_packet * sizeof(uint64_t)) == false) {
            vTaskDelay(pdMS_TO_TICKS(100)); // Protocol not ready, wait and retry
        }
        // give a binary here, that will cause LED to go high for 200ms

        // 4. Delay until the next packet transmission and increment counter
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
        packet_counter++;
    }
}

void loraMonitorTask(void* pvParameters){
    ESP_LOGI(TAG, "Lora Monitor Task started");
    RadioConfig cfg = getStandardConfig(BoardType::Ebyte_SX1262, TestMode::highPower);
    for(;;){
        //monitor for crashes
        char* errMsg = NULL;
        int16_t errCode = runProtocol(&cfg, errMsg);
        ESP_LOGE(TAG, "Lora Driver crashed with code %d, msg: %s", errCode, errMsg);
        vTaskDelay(pdMS_TO_TICKS(1000)); //check every second
    }
}

extern "C" void app_main(void) {
    vTaskDelay(pdMS_TO_TICKS(5000));
    xTaskCreateStatic(loraSendTask, "Lora_Send_Task", LORA_TASK_SIZES, NULL, 1, LORA_Send_Stack, &LORA_Send_Buffer);
    xTaskCreateStatic(loraMonitorTask, "Lora_Monitor_Task", LORA_TASK_SIZES, NULL, 1, LORA_Monitor_Stack, &LORA_Monitor_Buffer);

}
