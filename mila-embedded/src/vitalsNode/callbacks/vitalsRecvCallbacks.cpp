#include "../vitalsHelper/vitalsPacketRecvLUT.h"
// #include "vitals/vitals.h" // No longer needed here, forwarding handled in LUT
#include "esp_log.h"

static const char* TAG = "VitalsRecvCallbacks";

/**
 * @brief These are auto-generated stub implementations for the packet receiver callbacks.
 * This file is generated once and will not be overwritten.
 *
 * The code generator declares the function prototypes in vitalsPacketRecvLUT.h
 * and calls them from the parser in vitalsPacketRecvLUT.c.
 *
 * You can add your application-specific logic to these functions.
 * For packets not targeted at this "vitals" node, a forwarding implementation is provided.
 */

void ongenericVitalsCommand(genericVitalsCommand_args_t args) {
    ESP_LOGI(TAG, "Callback ongenericVitalsCommand called. vitalsCommands: %" PRId32, args.vitalsCommands.i32);
    // TODO: Implement logic for genericVitalsCommand
}

void onset_telem_update_frequency(set_telem_update_frequency_args_t args) {
    ESP_LOGI(TAG, "Callback onset_telem_update_frequency called. divider: %" PRId32, args.divider);
    // TODO: Implement logic for set_telem_update_frequency
}

void onprechargeCommand(prechargeCommand_args_t args) {
    ESP_LOGI(TAG, "Callback onprechargeCommand called. (packet was forwarded to prechargeID by Vitals). prechargeCommands: %" PRId32, args.prechargeCommands.i32);
    // TODO: Implement logic for prechargeCommand
}

void onprechargeValue(prechargeValue_args_t args) {
    ESP_LOGI(TAG, "Callback onprechargeValue called. (packet was forwarded to prechargeID by Vitals). value1: %" PRId32, args.value1);
    // TODO: Implement logic for prechargeValue
}

size_t onforward_packet(forward_packet_args_t args) {
    ESP_LOGI(TAG, "Callback onforward_packet called. CAN_ID: %" PRId32 ", dataLength: %" PRId32 ", extendedID: %" PRId32, args.CAN_ID, args.dataLength, args.extendedID);
    // TODO: Implement logic for forward_packet
    // Access payload via args.payload, with max size args.max_payload_size
    // Example: ESP_LOG_BUFFER_HEX(TAG, args.payload, args.max_payload_size);
    return 0; // Return number of payload bytes consumed
}

