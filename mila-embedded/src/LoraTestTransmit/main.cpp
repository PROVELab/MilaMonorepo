#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "esp_timer.h"

#include "../LoraCommon/Driver/Driver.hpp"

static const char* TAG= "main";

//Callbacks for protocol when RX/TX finish. Protocol must implement these
#define timeout_ms 2000
#define timeout_us (timeout_ms * 1000)

#define seqNum 9900

uint64_t counter = 1;

void protocolTXComplete(){

    ESP_LOGI(TAG, "transmit complete for num %llu", counter - 1);

    driverSendPacket msg= {{0}};
    *((uint16_t*) msg.data) = seqNum;
    uint64_t* writePtr = (uint64_t*) (msg.data + 2);
    const int loopSize = 30;//240 bytes of same long: 30*8 = 240
    for(int i = 0; i<loopSize;i++){   
        *writePtr = counter;
        writePtr ++;
    }
    msg.dataSize = sizeof(counter) * loopSize + 2;
    LoraTransmit(&msg,  esp_timer_get_time() + timeout_us);

    counter++;
}

void protocolRecv(const driverRecvPacket* packet){
    ESP_LOGE(TAG, "Warning protocolReceive called");
    ESP_LOGI(TAG, "Received LoRa Packet - Length: %u", packet->dataSize);
    printf("Payload (Hex): ");
    for (size_t i = 0; i < packet->dataSize; i++) {
        printf("%02X ", packet->data[i]);
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
    RadioConfig cfg = getStandardConfig(BoardType::Ebyte_SX1262, TestMode::lowPower);
    LoraDriverInit(&cfg);
    // LoraDriverInit(getStandardConfig(BoardType::Ebyte_SX1262, TestMode::lowPower));
    driverSendPacket msg= {{0}};
    *((uint64_t*) msg.data) = counter;
    msg.dataSize = sizeof(counter);
    LoraTransmit(&msg,  esp_timer_get_time() + timeout_us);
}

