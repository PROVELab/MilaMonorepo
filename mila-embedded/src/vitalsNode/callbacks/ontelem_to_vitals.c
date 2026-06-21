#include "../vitalsGen/vitalsPacketRecvLUT.h"
#include "esp_log.h"
#include "pecan/pecan.h"
#include "../../programConstants.h"
#include <inttypes.h>
#include <string.h>

#include "../contactorControl.h"

static const char* TAG = "VitalsRecvCallbacks";

void ontelem_to_vitals(telem_to_vitals_args_t args) {
    ESP_LOGI(TAG, "Callback ontelem_to_vitals called. telem_to_vitals_Commands: %" PRId32, args.telem_to_vitals_Commands.i32);

    switch (args.telem_to_vitals_Commands.e) {
        case lowPowerLora:
            ESP_LOGI(TAG, "Received command: lowPowerLora");
            // TODO: Implement logic to switch LoRa to low power mode.
            break;
        case highPowerLora:
            ESP_LOGI(TAG, "Received command: highPowerLora");
            // TODO: Implement logic to switch LoRa to high power mode.
            break;
        case enablePrechargeIfSafe:
            ESP_LOGI(TAG, "Received manual contactor enable request from telem");
            (void)enableContactorsIfSafe();
            break;
        case disablePrecharge:
            ESP_LOGI(TAG, "Received manual contactor disable request from telem");
            sendContactorControlCommand(disableContactors);
        break;
        default:
            ESP_LOGW(TAG, "Received unknown telem_to_vitals command: %" PRId32, args.telem_to_vitals_Commands.i32);
            break;
    }
}
