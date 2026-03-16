#include <stdint.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <atomic>
#include <stdbool.h>

#include "../pecan/pecan.h"

#include "../LoraCommon/blastProtocolConfig.hpp"
#include "LoraTransmitQueue.hpp"
#include "../LoraCommon/LoraErrLog.hpp"
#include "TXBlastProtocolHelper.hpp"
#include "../LoraCommon/Driver/Driver.hpp"

static const char* TAG = "Blast_Protocol";

// For receiving acks
uint32_t packetTimeOnAir_us = 1; //computed in initProtocol

//we will first send with ackParity = false
bool ackParity = false; //parity bit to track acks. Ensure we know if we got old ack

//queue for incomming packets
#define queueSize 3
QueueHandle_t recvQueue = NULL;
StaticQueue_t xQueueBuffer;
//how much data for protocol packets
#define recvMsgDataSize (sizeof(driverPacket) - TXHeaderSize);
uint8_t ucQueueStorage[ queueSize * sizeof(driverPacket)];
//

ProtocolState state = unstarted;   //track where we are at in the protocol. Mainly for handling timeouts and crashes

static void initProtocol(const RadioConfig* config);
//non-reentrant
int16_t runProtocol(const RadioConfig* config, char*& errorMsg){
    state = unstarted;
    driverInfo* driverInfo;
    while(1){
        switch(state){
            case unstarted:
                initProtocol(config);
                state = idle;
                break;
            case startNewBurst:
                //will block until a data is ready if none
                state = prepareNewBurst();
                break;
            case sendingData:
                state = sendData();
                break;
            case awaitingAck:
                state = awaitAck(); //can call processBitmap
                break;
            case resendLastData:
                state = resendLastPacketInBurst();
                break;            
            case crashed:
                driverInfo = getDriverInfo();
                errorMsg = driverInfo->crashMsg;
                return driverInfo->crashError;
            default:
                return -1;
        }
    }
}

void initProtocol(const RadioConfig* config){
    ESP_LOGI(TAG, "called Init");
    if(recvQueue == NULL){
        recvQueue = xQueueCreateStatic( queueSize, // The number of items the queue can hold.
                        sizeof(driverPacket),     // The size of each item in the queue
                        ucQueueStorage, // The buffer that will hold the items in the queue.
                        &xQueueBuffer );
    }
    LoraDriverInit(config); //start driver

    //initialize things in other files. 
    initQueue();    //init and re-init are the same for these
    initErr();

    ackParity = !ackParity;
    packetTimeOnAir_us = LoraGetTimeOnAir(); //compute time on air for max size packet

    return ;
}

//false if protocol not running 
bool protocolTransmit(uint8_t* data, uint8_t dataLen){
    addFrameToQueue(data, dataLen);
    return true;
}

//not implementing for now
// bool protocolTransmitPriority(uint8_t* data, uint8_t dataLen){
//
// }
