#include "vitalsHelper.h"
#include "../vitalsGen/vitalsPacketSendLUT.h"
#include "esp_log.h"
#include <inttypes.h>
#include <string.h>

static const char* TAG = "SimpleForwarder";

int16_t forwardStatusUpdate(CANPacket* message) {
    if (message->dataSize < 1) {
        ESP_LOGW(TAG, "Received statusUpdate with no data from node %" PRIu32, getNodeId(message->id));
        return 1;
    }

    sendnodeStatusArgs args = {
        .mask = 0, // will be set by send function
        .nodeID = (int32_t)getNodeId(message->id),
        .statusUpdates = { .i32 = message->data[0] }
    };
    sendnodeStatusFunction(args);

    ESP_LOGI(TAG, "Forwarded status update from node %" PRIu32 " with status %u", getNodeId(message->id), message->data[0]);
    return 0;
}

int16_t vitals_defaultPacketRecv(CANPacket* p) {
    ESP_LOGW(TAG, "Received unhandled CAN packet with ID: 0x%" PRIX32, p->id);

    sendunknownCanPacketHeader header;
    header.nodeID = p->id & 0x7FF; // Standard 11-bit ID
    header.DLC = p->dataSize;
    header.extendedIDPresent = p->extendedID;
    header.RTR = p->rtr;

    uint32_t extension = getIdExtension(p->id);
    header.ext_id_start = (extension >> 16) & 0x3; // Top 2 bits of 18-bit extension

    uint8_t payload[10]; // At most 2 bytes for ext_id + 8 bytes for data
    size_t payload_len = 0;

    if (p->extendedID) {
        uint16_t ext_id_end = extension & 0xFFFF; // Bottom 16 bits
        // Pack as little-endian
        payload[0] = ext_id_end & 0xFF;
        payload[1] = (ext_id_end >> 8) & 0xFF;
        payload_len += 2;
    }

    if (!p->rtr && p->dataSize > 0) {
        memcpy(payload + payload_len, p->data, p->dataSize);
        payload_len += p->dataSize;
    }

    sendunknownCanPacketFunction(header, payload, payload_len);

    return 0;
}