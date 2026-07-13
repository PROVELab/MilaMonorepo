#include "../vitalsGen/vitalsPacketRecvLUT.h"
#include "esp_log.h"

static const char* TAG = "VitalsRecvCallbacks";

void onsetCoolantDutyCycle(setCoolantDutyCycle_args_t args) {
    ESP_LOGI(TAG, "Callback onsetCoolantDutyCycle called. (packet was forwarded to powerDistribution by Vitals). dutyCycle: %" PRId32, args.dutyCycle);
    // TODO: Implement logic for setCoolantDutyCycle
}
