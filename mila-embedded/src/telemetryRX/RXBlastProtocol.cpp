#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <atomic>
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

#include "RXBlastProtocolHelper.hpp"
#include "../LoraCommon/LoraProtocol.hpp"
#include "../LoraCommon/blastProtocolConfig.hpp"
#include "../LoraCommon/Driver/Driver.hpp"
// Define a tag for logging
static const char* TAG = "RX_Blast";

#include "RadioLib.h"

// For receiving acks
uint32_t packetTimeOnAir_us = 1; //computed in initProtocol

ProtocolState protocolState = unStarted;   //track where we are at in the protocol. Mainly for handling timeouts and crashes

//queue for incomming packets to give to the user
#define queueSize 8
QueueHandle_t recvQueue = NULL;
StaticQueue_t xQueueBuffer;
uint8_t ucQueueStorage[ queueSize * sizeof(driverPacket)];
//

//the ack packet
RXProtocolPacket responsePacket;

//set by awaitData
bool paritySet = false;
bool recordedAckParity = 0; 
//

bool protocolTransmit(uint8_t* data, uint8_t dataLen){
    return false;
}

bool protocolRecv(driverPacket* msg){
    if(!recvQueue){
        return false;
    }
    const int maxTime_Seconds = 10;
    if(xQueueReceive(recvQueue, msg, pdMS_TO_TICKS(maxTime_Seconds * 1000))!=pdPASS){
        return false;
    }
    return true;
}

static void initProtocol(const RadioConfig* config);
static ProtocolState sendAck();
//non-reentrant
int16_t runProtocol(const RadioConfig* config, char*& errorMsg){
    protocolState = unStarted;
    driverInfo* driverInfo;
    while(1){
        switch(protocolState){
            case unStarted:
                initProtocol(config);
                protocolState = awaitingData;
                break;
            case crashed:
                driverInfo = getDriverInfo();
                errorMsg = driverInfo->crashMsg;
                return driverInfo->crashError;
            case awaitingData:
                protocolState = awaitData();
                break;
            case sendingAck:
                protocolState = sendAck();
                break;
            default:
                return -1;
        }
    }
}

void initProtocol(const RadioConfig* config){
    if(recvQueue == NULL){
        recvQueue = xQueueCreateStatic( queueSize, // The number of items the queue can hold.
                        sizeof(driverPacket),     // The size of each item in the queue
                        ucQueueStorage, // The buffer that will hold the items in the queue.
                        &xQueueBuffer );
    }
    LoraDriverInit(config); //start driver

    paritySet = false;

    packetTimeOnAir_us = LoraGetTimeOnAir(); //compute time on air for max size packet

    return ;
}


ProtocolState sendAck(){
    //TODO: add a means for user to send their own stuff, instead of just dataSize = 0.
    //send an ack with the same parity bit as the last packet we got, so the other side knows we got it
    responsePacket.dataSize = RXHeaderSize;   //just send a 1 byte packet for the ack, with the parity bit to indicate which packet we are acking
    responsePacket.protocolID = protocolUniqueID;
    responsePacket.flags = recordedAckParity ? 0x01 : 0x00;
    //ok to block for a very long time to get this packet queued:
    const uint64_t timeoutTime_us = esp_timer_get_time() + ((maxPacketGroupSize * packetTimeOnAir_us) * 8);
    if((LoraTransmit((driverPacket*) &responsePacket, timeoutTime_us)) != RADIOLIB_ERR_NONE){
        ESP_LOGE(TAG, "cant queue transmission");
        return crashed;
    }
    return awaitingData;
}
