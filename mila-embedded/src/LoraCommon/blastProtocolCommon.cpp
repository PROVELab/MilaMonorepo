#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <atomic>
#include <cstring>
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "LoraErrLog.hpp"

#include "../LoraCommon/blastProtocolConfig.hpp"
#include "../LoraCommon/Driver/Driver.hpp"
#include "LoraProtocol.h"
// Define a tag for logging
static const char* TAG = "Blast_Common";

// Print irq flags for debugging.
void printRecvStatus(driverRecvPacket* info) {
     ESP_LOGI(TAG, "IRQ Flags (0x%04zX): [", info->irqFlags);

    //log the strength of incomming packet
    ESP_LOGI(TAG, "Packet Recv - RSSI: %.2f dBm, SNR: %.2f dB", 
                info->RSSI, info->SNR);
    //choosing not to error check these^^. not worth crashing on them, if other things magically work.
}

//returns true if this is a valid packet
bool validatePacketHeader(driverRecvPacket* driverPacket, size_t expectedHeaderLength) {
    // To safely read a multi-byte value from a raw byte buffer and avoid
    // strict-aliasing violations or unaligned access crashes, we use std::memcpy.
    if (driverPacket->dataSize < sizeof(protocolID_t)) {
        logErr(TAG, invalidRXLength);
        return false;
    }

    protocolID_t p_id;
    std::memcpy(&p_id, driverPacket->data, sizeof(protocolID_t));
    if (p_id != protocolUniqueID) {
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

bool LoraDriverRunning(){
    driverInfo* info = getDriverInfo();
    return info->state == running;
}