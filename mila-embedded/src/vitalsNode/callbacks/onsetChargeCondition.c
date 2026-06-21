#include "../vitalsGen/vitalsPacketRecvLUT.h"
#include "esp_log.h"

static const char* TAG = "VitalsRecvCallbacks";

void onsetChargeCondition(setChargeCondition_args_t args) {
    ESP_LOGI(TAG, "Callback onsetChargeCondition called. (packet was forwarded to precharge by Vitals). min_MC_Voltage: %" PRId32 ", minPercentCharged: %" PRId32, args.min_MC_Voltage, args.minPercentCharged);
    // TODO: Implement logic for setChargeCondition
}
