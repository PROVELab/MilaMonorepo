
#include <stdint.h>

#include "esp_log.h"
#include "../LoraCommon/LoraErrLog.hpp"

//RX side can just use ESP_LOGE, to send directly to dashboard
void initErr(){ 

}

void logErr(const char* TAG, int16_t err){
    ESP_LOGE(TAG, "RX Lora Err code raised: %d", err);
}

//only used by TX side atm
uint8_t getErrorPacket(int16_t* errPacket, uint8_t maxErrCount) = delete;
