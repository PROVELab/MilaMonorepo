#include "../vitalsGen/vitalsPacketRecvLUT.h"
#include "esp_log.h"

static const char* TAG = "VitalsRecvCallbacks";

void onsetCoolantFrequency_HZ(setCoolantFrequency_HZ_args_t args) {
    ESP_LOGI(TAG, "Callback onsetCoolantFrequency_HZ called. (packet was forwarded to powerDistribution by Vitals). frequency_HZ: %" PRId32, args.frequency_HZ);
    // TODO: Implement logic for setCoolantFrequency_HZ
}
