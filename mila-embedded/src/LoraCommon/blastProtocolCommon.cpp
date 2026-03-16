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
void printRecvStatus(driverPacket* info) {
     ESP_LOGI(TAG, "IRQ Flags (0x%04" PRIX32 "): [", info->irqFlags);
    for (int i = 31; i >= 0; i--) {
         ESP_LOGI(TAG, "%u", (unsigned int) ((info->irqFlags >> i) & 0x01));
        if (i % 4 == 0 && i != 0) printf(" ");
    }
     ESP_LOGI(TAG, "]\n");

    //log the strength of incomming packet
    ESP_LOGI(TAG, "Packet Recv - RSSI: %.2f dBm, SNR: %.2f dB", 
                info->RSSI, info->SNR);
    //choosing not to error check these^^. not worth crashing on them, if other things magically work.
}

//returns true if this is a valid packet
bool validatePacketHeader(driverPacket* driverPacket) {
    RXProtocolPacket* protocolPacket = (RXProtocolPacket*) driverPacket->data;
    //Error checking
    //basic length cheks
    if(offsetof(RXProtocolPacket, protocolID) + sizeof(protocolID_t) > RXHeaderSize){
        logErr(invalidRXLength);
        return false;
    }
    if(protocolPacket->protocolID != protocolUniqueID) {
        logErr(incorrectProtocolId);
        return false;
    }
    if (driverPacket->dataSize < RXHeaderSize) {   //basic length check
        logErr(invalidRXLength);
        return false;
    }

    return true;    //packet looks good!
}
