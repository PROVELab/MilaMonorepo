/* Add to the ESP Pecan build, not to the Arduino Pecan build. */
#include "pecan/pecan.h"
#include "driver/twai.h"
#include <string.h>

bool trySendPacket(const CANPacket *p) {
    if (!p || p->dataSize > 8 || p->id < 0 || p->id > 0x7ff || p->extendedID)
        return false;
    twai_message_t message = {0};
    message.identifier = (uint32_t)p->id;
    message.rtr = p->rtr;
    message.data_length_code = p->dataSize;
    memcpy(message.data,p->data,p->dataSize);
    /* Queued is not the same as delivered. No blocking retries in this API. */
    return twai_transmit(&message,0) == ESP_OK;
}
