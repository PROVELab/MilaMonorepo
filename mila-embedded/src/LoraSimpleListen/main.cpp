#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include "../LoraCommon/LoraDriver.hpp"

static const char* TAG= "main";

// #define LORA_TASK_SIZES 4096

// StaticTask_t LORA_Monitor_Buffer;
// StackType_t LORA_Monitor_Stack[LORA_TASK_SIZES];

//Callbacks for protocol when RX/TX finish. Protocol must implement these
void protocolTXComplete(){
    ESP_LOGE(TAG, "protocolTXComplete called. We are listen only. not sure how this happened");
}
void protocolReceive(const uint8_t* rxBuffer, const size_t length){
    ESP_LOGI(TAG, "Received LoRa Packet - Length: %u", length);
    printf("Payload (Hex): ");
    for (size_t i = 0; i < length; i++) {
        printf("%02X ", rxBuffer[i]);
    }
    printf("\n");
    LoraStartRecv();
}

//Callback for when driver Crashes. Protocol must implement these
void protocolCrash(const int16_t error, const char* msg){
    ESP_LOGE(TAG, "protocolCrash called from %s with error %d", msg, error);
}    //could run LoraDriverRestart() here if want to try to recover.

extern "C" void app_main(void) {
    vTaskDelay(pdMS_TO_TICKS(2000)); //wait 2 seconds before starting RX
    ESP_LOGI(TAG, "Starting Lora RX from main");

  // loop forever
//   LoraDriverInit(getStandardConfig(BoardType::Wio_SX1262, TestMode::highPower));
  LoraDriverInit(getStandardConfig(BoardType::Ebyte_SX1262, TestMode::highPower));
  LoraStartRecv();
}

