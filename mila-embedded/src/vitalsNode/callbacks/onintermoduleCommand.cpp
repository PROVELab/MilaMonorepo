#include "../vitalsGen/vitalsPacketRecvLUT.h"
// #include "vitals/vitals.h" // No longer needed here, forwarding handled in LUT
#include "esp_log.h"

static const char* TAG = "VitalsRecvCallbacks";

void onintermoduleCommand(intermoduleCommand_args_t args) {
    ESP_LOGI(TAG, "Callback onintermoduleCommand called. (packet was forwarded to powerDistribution by Vitals). prechargeCommands: %" PRId32, args.prechargeCommands.i32);
    // TODO: Implement logic for intermoduleCommand
}
