#include "../vitalsGen/vitalsPacketRecvLUT.h"
// #include "vitals/vitals.h" // No longer needed here, forwarding handled in LUT
#include "esp_log.h"
#include "../../programConstants.h"

static const char* TAG = "VitalsRecvCallbacks";

void onprechargeCommand(prechargeCommand_args_t args) {
    ESP_LOGI(TAG, "Callback onprechargeCommand called. (packet was forwarded to prechargeID by Vitals). prechargeCommands: %" PRId32, args.prechargeCommands.i32);
    
    // This callback on the Vitals node is for logging/confirmation purposes.
    // The command has already been forwarded to the precharge node by the auto-generated wrapper.
    // The actual logic should be implemented on the precharge node itself.
    switch (args.prechargeCommands.e) {
        case prechargeRemoveLatch:
            ESP_LOGI(TAG, "Forwarding command: prechargeRemoveLatch");
            break;
        case prechargeLatchOff:
            ESP_LOGI(TAG, "Forwarding command: prechargeLatchOff");
            break;
        case prechargeLatchOn:
            ESP_LOGI(TAG, "Forwarding command: prechargeLatchOn");
            break;
        default:
            ESP_LOGW(TAG, "Forwarding unknown prechargeCommand: %" PRId32, args.prechargeCommands.i32);
            break;
    }
}
