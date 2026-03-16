
#include <stdint.h>

#include "esp_log.h"
#include "../LoraCommon/LoraErrLog.hpp"

const char* TAG = "LoraErrLog";
//RX side can just use ESP_LOGE, to send directly to dashboard
void initErr(){ 

}

void logErr(int16_t err){
    ESP_LOGE(TAG, "Err: %d", err);
}