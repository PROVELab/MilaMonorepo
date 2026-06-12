#include "../vitalsGen/vitalsPacketRecvLUT.h"
// #include "vitals/vitals.h" // No longer needed here, forwarding handled in LUT
#include "esp_log.h"
#include "../../programConstants.h"

static const char* TAG = "VitalsRecvCallbacks";

void ongenericVitalsCommand(genericVitalsCommand_args_t args) {
    ESP_LOGI(TAG, "Callback ongenericVitalsCommand called. vitalsCommands: %" PRId32, args.vitalsCommands.i32);
    
    switch (args.vitalsCommands.e) {
        case lowPowerLora:
            ESP_LOGI(TAG, "Received command: lowPowerLora");
            // TODO: Implement logic to switch LoRa to low power mode.
            break;
        case highPowerLora:
            ESP_LOGI(TAG, "Received command: highPowerLora");
            // TODO: Implement logic to switch LoRa to high power mode.
            break;
        default:
            ESP_LOGW(TAG, "Received unknown vitalsCommand: %" PRId32, args.vitalsCommands.i32);
            break;
    }
}
