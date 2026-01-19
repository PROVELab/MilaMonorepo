#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "esp_timer.h"

#include "../LoraCommon/LoraDriver.hpp"

static const char* TAG= "main";

//Callbacks for protocol when RX/TX finish. Protocol must implement these
#define timeout_ms 2000
#define timeout_us (timeout_ms * 1000)

uint64_t counter = 1;

void protocolTXComplete(){

    ESP_LOGI(TAG, "transmit complete for num %llu", counter - 1);

    LoraTransmit((uint8_t*) (&counter), sizeof(counter), esp_timer_get_time() + timeout_us);

    counter++;
}

void protocolReceive(const uint8_t* rxBuffer, const size_t length){
    ESP_LOGE(TAG, "Warning protocolReceive called");
    ESP_LOGI(TAG, "Received LoRa Packet - Length: %u", length);
    printf("Payload (Hex): ");
    for (size_t i = 0; i < length; i++) {
        printf("%02X ", rxBuffer[i]);
    }
    printf("\n");
}

//Callback for when driver Crashes. Protocol must implement these
void protocolCrash(const int16_t error, const char* msg){
    ESP_LOGE(TAG, "protocolCrash called from %s with error %d", msg, error);
}    //function to be called by driver to crash

extern "C" void app_main(void) {
    vTaskDelay(pdMS_TO_TICKS(2000)); //wait 2 seconds before starting TX
    ESP_LOGI(TAG, "Starting Lora TX from main");
    // loop forever
    LoraDriverInit(getStandardConfig(BoardType::Ebyte_SX1262, TestMode::highPower));
    // LoraDriverInit(getStandardConfig(BoardType::Ebyte_SX1262, TestMode::lowPower));
    LoraTransmit((uint8_t*) (&counter), sizeof(counter), esp_timer_get_time() + timeout_us);
}

