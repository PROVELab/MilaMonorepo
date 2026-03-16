#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include "../LoraCommon/Driver/Driver.hpp"

static const char* TAG= "main";

// #define LORA_TASK_SIZES 4096
#define seqNum 9900

// StaticTask_t LORA_Monitor_Buffer;
// StackType_t LORA_Monitor_Stack[LORA_TASK_SIZES];

//Callbacks for protocol when RX/TX finish. Protocol must implement these
void protocolTXComplete(){
    ESP_LOGE(TAG, "protocolTXComplete called. We are listen only. not sure how this happened");
}
static int64_t firstNum = -1;
static int64_t lastNum = -1;
static int64_t recvCount = 0;
void protocolRecv(const driverPacket* packet){
    // ESP_LOGI(TAG, "Received LoRa Packet - Length: %u", packet->dataSize);
    uint16_t recvSeqNum = *((uint16_t*)packet->data);
    if(recvSeqNum == seqNum){
        if(packet->dataSize != (30 * 8) + 2){
            printf("invalid packet size??\n");
            return;
        }
        uint64_t* readPtr = (uint64_t*)(packet->data + 2);
        uint64_t prevValue = *readPtr;
        readPtr++;
        for(int i = 0; i< 29; i++){
            uint64_t value = *readPtr;
            if(value != prevValue){
                printf("inconsistentData\n");
                return;
            }
            prevValue = value;
            readPtr++;
        }
        if(firstNum == -1){
            firstNum = prevValue;
        }
        if(lastNum >= prevValue && lastNum != -1){
            printf("Warning, packets out of order. should never happen, results prob cooked\n");
        }
        recvCount ++;
        printf("recv val: %lld. Ratio: (%lld/%llu)\n", prevValue, recvCount, (uint64_t)(prevValue - firstNum  + 1));
        lastNum = prevValue;

    }else{
        printf("Warning Invalid SeqNum:\n Payload (Hex): ");
        for (size_t i = 0; i < packet->dataSize; i++) {
            printf("%02X ", packet->data[i]);
        }
        printf("\n");
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
    RadioConfig cfg = getStandardConfig(BoardType::Ebyte_SX1262, TestMode::highPower);
    LoraDriverInit(&cfg);
  LoraStartRecv();
}

