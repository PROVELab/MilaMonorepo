#include "../vitalsGen/vitalsPacketRecvLUT.h"
// #include "vitals/vitals.h" // No longer needed here, forwarding handled in LUT
#include "esp_log.h"
#include "pecan/pecan.h"
#include <string.h>

static const char* TAG = "VitalsRecvCallbacks";

size_t onforward_packet(forward_packet_args_t args) {
    ESP_LOGI(TAG, "Callback onforward_packet called. CAN_ID: %" PRId32 ", dataLength: %" PRId32, args.CAN_ID, args.dataLength);

    CANPacket p;
    memset(&p, 0, sizeof(CANPacket));

    // NOTE: This implementation assumes standard 11-bit CAN IDs.
    // The 'extendedID' field is ignored for now due to ambiguity in payload format.
    p.id = args.CAN_ID;
    p.extendedID = false; // Forcing standard ID
    p.dataSize = args.dataLength;

    if (p.dataSize > MAX_SIZE_PACKET_DATA) {
        ESP_LOGE(TAG, "forward_packet: dataLength %" PRId32 " exceeds max CAN data size of %d.", args.dataLength, MAX_SIZE_PACKET_DATA);
        return 0; // Indicate error by consuming 0 bytes.
    }

    if (p.dataSize > 0) {
        if (args.payload == NULL || args.max_payload_size < p.dataSize) {
            ESP_LOGE(TAG, "forward_packet: Insufficient payload for dataLength %u.", p.dataSize);
            return 0; // Error
        }
        memcpy(p.data, args.payload, p.dataSize);
    }

    sendPacket(&p);

    // The payload of the 'forward_packet' telemetry message is the data for the CAN frame.
    // We consumed 'dataLength' bytes from it.
    return p.dataSize;
}
