#include "../vitalsGen/vitalsPacketRecvLUT.h"
#include "../vitalsGen/vitalsStructs.h"
#include "../vitalsGen/vitalsPacketSendLUT.h" // For vitals_send_rate_controllers
#include "../../programConstants.h"
#include "esp_log.h"
#include <inttypes.h>

static const char* TAG = "VitalsRecvCallbacks";

void onset_telem_update_frequency(set_telem_update_frequency_args_t args) {
    ESP_LOGI(TAG, "Callback onset_telem_update_frequency called. nodeID: %" PRId32 ", packet_or_frame_ID: %" PRId32 ", divider: %" PRId32,
             args.nodeID, args.packet_or_frame_ID, args.divider);

    if (args.nodeID == vitalsID) {
        // This command is for a vitals-to-telemetry packet
        int32_t packet_idx = args.packet_or_frame_ID;
        if (packet_idx >= 0 && packet_idx < numVitalsToTelemPackets) {
            vitals_send_rate_controllers[packet_idx].divider = (args.divider > 0) ? args.divider : 1; // Ensure divider is at least 1
            vitals_send_rate_controllers[packet_idx].counter = 0; // Reset counter
            ESP_LOGI(TAG, "Updated vitals-to-telem packet (index %" PRId32 ") divider to %" PRId32, packet_idx, args.divider);
        } else {
            ESP_LOGE(TAG, "Invalid packet_idx %" PRId32 " for vitals packet frequency update", packet_idx);
        }
    } else {
        // This command is for a sensor node's CAN frame
        // Find the node index corresponding to the nodeID
        for (int i = 0; i < numberOfNodes; i++) {
            // Assuming the first frame's nodeID is representative for the whole vitalsNode
            if (nodes[i].CANFrames[0].nodeID == args.nodeID) {
                // Found the node. Now validate the frameID.
                int frame_id = args.packet_or_frame_ID;
                if (frame_id >= 0 && frame_id < nodes[i].numFrames) {
                    nodes[i].CANFrames[frame_id].telemetryDivider = args.divider;
                    ESP_LOGI(TAG, "Updated telemetry divider for node %" PRId32 " frame %d to %" PRId32,
                             args.nodeID, frame_id, args.divider);
                    return;
                } else {
                    ESP_LOGE(TAG, "Invalid frameID %d for node %" PRId32, frame_id, args.nodeID);
                    return;
                }
            }
        }
        ESP_LOGE(TAG, "NodeID %" PRId32 " not found for set_telem_update_frequency", args.nodeID);
    }
}