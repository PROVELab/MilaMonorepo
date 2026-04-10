#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <atomic>
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "LoraErrLog.hpp"

#include "../LoraCommon/blastProtocolConfig.hpp"
#include "../LoraCommon/Driver/Driver.hpp"
// Define a tag for logging
static const char* TAG = "Blast_Common";

// Print irq flags for debugging.
void printRecvStatus(driverRecvPacket* info) {
     ESP_LOGI(TAG, "IRQ Flags (0x%04" PRIX32 "): [", info->irqFlags);

    //log the strength of incomming packet
    ESP_LOGI(TAG, "Packet Recv - RSSI: %.2f dBm, SNR: %.2f dB", 
                info->RSSI, info->SNR);
    //choosing not to error check these^^. not worth crashing on them, if other things magically work.
}

//returns true if this is a valid packet
bool validatePacketHeader(driverRecvPacket* driverPacket, size_t expectedHeaderLength) {
    // The protocolID is at the start of both TX and RX packets.
    protocolID_t* p_id = (protocolID_t*) driverPacket->data;
    if(*p_id != protocolUniqueID) {
        logErr(TAG, incorrectProtocolId);
        return false;
    }

    // The smallest valid packet is a header with no payload.
    if (driverPacket->dataSize < expectedHeaderLength) {
        logErr(TAG, invalidRXLength);
        return false;
    }

    return true;    //packet looks good!
}
