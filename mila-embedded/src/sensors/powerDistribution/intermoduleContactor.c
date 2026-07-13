#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "../../pecan/pecan.h"
#include "../../programConstants.h"
#include "esp_log.h"

#include "helper/myDefines.hpp"

const char* TAG = "intermoduleContactor";

// No physical intermodule contactors yet. Acknowledge enable requests so Vitals can advance.

int16_t handleVitalsCommand(CANPacket* p) {
    if (p->dataSize < 1) {
        ESP_LOGW(TAG, "recv Vitals command with payload size <1. need at least 1 byte to read command");
        return 1;
    }

    if (p->data[0] == enableContactor) {
        ESP_LOGI(TAG, "Received enableContactor from Vitals; reporting success without hardware contactors");
        sendStatusUpdate(contactorsSuccess, myId);
        return SUCCESS;
    }

    if (p->data[0] == disableContactor) {
        ESP_LOGI(TAG, "Received disableContactor from Vitals");
        return SUCCESS;
    }

    ESP_LOGW(TAG, "Unknown Vitals contactor command: %u", (unsigned)p->data[0]);
    return GEN_FAILURE;
}

void registerVitalsContactorHandler(PCANListenParamsCollection* plpc) {
    CANListenParam vitalsCommandParam = {0};
    vitalsCommandParam.listen_id = combinedID(vitalsCommand, myId);
    vitalsCommandParam.handler = handleVitalsCommand;
    vitalsCommandParam.mt = MATCH_EXACT;

    if (addParam(plpc, vitalsCommandParam) != SUCCESS) {
        ESP_LOGE(TAG, "Failed to add vitals contactor handler");
    }
}
