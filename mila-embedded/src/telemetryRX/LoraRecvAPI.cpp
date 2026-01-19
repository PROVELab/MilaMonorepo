#include "LoraRecvAPI.hpp"
#include "freertos/queue.h"
#include "freertos/FreeRTOS.h"

void Lora_RX_Init(const RadioConfig config){
    RX(config);
}
void Lora_RX_Restart(const RadioConfig config){
    initRecvQueue(); //clear queue
    LoraDriverRestart(config);
}


