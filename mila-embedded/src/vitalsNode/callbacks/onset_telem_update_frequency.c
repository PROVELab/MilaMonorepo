#include "../vitalsGen/vitalsPacketRecvLUT.h"
#include "../vitalsGen/vitalsStructs.h"
#include "../vitalsGen/vitalsPacketSendLUT.h" // For vitals_send_rate_controllers
#include "../../programConstants.h"
#include "esp_log.h"
#include <inttypes.h>

#include "../vitalsData/vitalsData.h"
#include "../vitalsHelper/vitalsHelper.h"

static const char* TAG = "VitalsRecvCallbacks";

void onset_telem_update_frequency(set_telem_update_frequency_args_t args) {
    ESP_LOGI(TAG, "Callback onset_telem_update_frequency called. nodeID: %" PRId32 ", packet_or_frame_ID: %" PRId32 ", divider: %" PRId32,
             args.nodeID, args.packet_or_frame_ID, args.divider);

    if (args.nodeID == vitalsID) {
        // This command is for a vitals-to-telemetry packet
        int32_t packet_idx = args.packet_or_frame_ID;
        if (packet_idx >= 0 && packet_idx < numVitalsToTelemPackets) {
            // Update divider, while ensuring it is at least 1
            vitals_send_rate_controllers[packet_idx].divider = (args.divider > 0) ? args.divider : 1; 
            // Reset counter
            vitals_send_rate_controllers[packet_idx].counter = 0; 
            ESP_LOGI(TAG, "Updated vitals-to-telem packet (index %" PRId32 ") divider to %" PRId32, packet_idx, args.divider);
        } else {
            ESP_LOGE(TAG, "Invalid packet_idx %" PRId32 " for vitals packet frequency update", packet_idx);
        }
        return;
    } 

    // This command is for a sensor node's CAN frame
    // Find the node index corresponding to the nodeID
    uint32_t nodeIndex = IDTovitalsIndex(args.nodeID);
    if (nodeIndex == invalidVitalsIndex){
        ESP_LOGE(TAG, "Invalid nodeID %" PRId32 " for set_telem_update_frequency", args.nodeID);
        return;
    }
    // Found the node. Now validate the frameID.
    const int32_t frame_id = args.packet_or_frame_ID;
    if(frame_id < 0 || frame_id >= nodes[nodeIndex].numFrames){
        ESP_LOGE(TAG, "Invalid frameID %" PRIi32 " for node %" PRId32 " not updating telem divider", frame_id, args.nodeID);
        return;
    }

    //update the divider
    const uint32_t newDivider = (args.divider > 0) ? args.divider : 1;
    if(updateTelemetryDivider((nodes[nodeIndex].CANFrames) + frame_id, newDivider)){
        ESP_LOGI(TAG, "Updated telemetry divider for node %" PRId32 " frame %" PRIi32 " to %" PRIu32,
                args.nodeID, frame_id, newDivider);
    
    }
}
