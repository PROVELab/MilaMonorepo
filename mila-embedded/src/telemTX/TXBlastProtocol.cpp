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
bool firstBurst = true;

//queue for incomming packets
#define queueSize 3
QueueHandle_t recvQueue = NULL;
StaticQueue_t xQueueBuffer;
//how much data for protocol packets
#define recvMsgDataSize (maxLoraPacketSize - TXHeaderSize);
uint8_t ucQueueStorage[ queueSize * sizeof(driverRecvPacket) ];
//

ProtocolState state = unstarted;   //track where we are at in the protocol. Mainly for handling timeouts and crashes

static void initProtocol(const RadioConfig* config);
//non-reentrant
extern "C" int16_t runProtocol(const RadioConfig* config, char** errorMsg){
    state = unstarted;
    driverInfo* driverInfo;
    while(1){
        switch(state){
            case unstarted:
                initProtocol(config);
                state = idle;
                break;
            case idle:
                // This state can be used to wait for a trigger. For now, go straight to sending.
                // prepareNewBurst() will block until data is available in the queue.
                state = startNewBurst;
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
                *errorMsg = driverInfo->crashMsg;
                return driverInfo->crashError;
            default:
                return -1;
        }
    }
}

static void initProtocol(const RadioConfig* config){
    ESP_LOGI(TAG, "called Init");
    if(recvQueue == NULL){
        recvQueue = xQueueCreateStatic( queueSize, // The number of items the queue can hold.
                        sizeof(driverRecvPacket),     // The size of each item in the queue
                        ucQueueStorage, // The buffer that will hold the items in the queue.
                        &xQueueBuffer );
    }
    //initialize things in other files. 
    initErr();
    firstBurst = true;
    ackParity = !ackParity; // Toggle parity on each restart to help RX resets its burst tracking

    LoraDriverInit(config); //start driver

    packetTimeOnAir_us = LoraGetTimeOnAir(); //compute time on air for max size packet

    return ;
}

TXQueue queue;
//false if protocol not running 
extern "C" bool protocolTransmit(uint8_t* data, uint8_t dataLen){
    if(state == crashed  || state == unstarted) return false;
    return queue.addFrameToQueue(data, dataLen);
}

extern "C" void getBitmap(uint16_t* bitmap, uint8_t* burstSize){
    *bitmap = 0;
    *burstSize = 0;
}

//not implementing for now
// bool protocolTransmitPriority(uint8_t* data, uint8_t dataLen){
//
// }

extern "C" bool protocolRecv(driverRecvPacket* msg){
    if(state == crashed || state == unstarted) return false;

    const int maxTime_Seconds = 20;

    if(xQueueReceive(recvQueue, msg, pdMS_TO_TICKS(maxTime_Seconds * 1000)) == pdPASS){
        return true;
    }
    return false;
}