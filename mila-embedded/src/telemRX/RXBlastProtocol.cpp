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
#include "../LoraCommon/LoraProtocol.h"
#include "../LoraCommon/blastProtocolConfig.hpp"
#include "../LoraCommon/Driver/Driver.hpp"
// Define a tag for logging
static const char* TAG = "RX_Blast";

#include "RadioLib.h"

// For receiving acks
uint32_t packetTimeOnAir_us = 1; //computed in initProtocol

ProtocolState protocolState = unStarted;   //track where we are at in the protocol. Mainly for handling timeouts and crashes

burstStateContainer burstState;

//queue for incomming packets to give to the user
#define queueSize 8
QueueHandle_t recvQueue = NULL;
StaticQueue_t xQueueBuffer;
uint8_t ucQueueStorage[ queueSize * sizeof(driverRecvPacket)];
//

extern "C" bool protocolRecv(driverRecvPacket* msg){
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
//non-reentrant
extern "C" int16_t runProtocol(const RadioConfig* config, char** errorMsg){
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
                *errorMsg = driverInfo->crashMsg;
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

static void initProtocol(const RadioConfig* config){
    if(recvQueue == NULL){
        recvQueue = xQueueCreateStatic( queueSize, // The number of items the queue can hold.
                        sizeof(driverRecvPacket),     // The size of each item in the queue
                        ucQueueStorage, // The buffer that will hold the items in the queue.
                        &xQueueBuffer );
    }
    initSendBuffer();

    burstState.recordedAckParity = false;
    burstState.paritySet = false;
    burstState.latestBurstSize = 0;
    burstState.lastFrameRecv = -1;
    burstState.firstBurstEncounter = false;
    burstState.bitmap = 0;
    burstState.last_sent_ack_bitmap = 0;
    burstState.last_sent_ack_burst_size = 0;

    LoraDriverInit(config); //start driver

    packetTimeOnAir_us = LoraGetTimeOnAir(); //compute time on air for max size packet

    return;
}
