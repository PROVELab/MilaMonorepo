#include <stdint.h>
#include <string.h>

#include "freertos/FreeRTOS.h"

#include "../LoraCommon/blastProtocolConfig.hpp"
#include "../LoraCommon/LoraErrLog.hpp"

static uint8_t TXQueue[maxPacketsInQueue][maxTXDataSize];
static uint8_t TXQueueLens[maxPacketsInQueue];

static uint16_t queueHead = 0, queueTail = 0, queueCount = 0;

static SemaphoreHandle_t queueMutex = NULL; //mutex to take or add to msg queue.

static SemaphoreHandle_t queueNonEmptyBinary = NULL;

SemaphoreHandle_t queueReadyBinary = NULL;   //binary to indicate when we have something in the queue ready to send. Given by protocol when it wants us to nudge it to send

//may be called again on restart. Guaranteed not to have other queue functions running when called.
void initQueue(){
    if(queueMutex == NULL){ 
        queueMutex = xSemaphoreCreateMutex();
    }
    if(queueNonEmptyBinary == NULL){
        queueNonEmptyBinary = xSemaphoreCreateBinary();
    }
    xSemaphoreTake(queueMutex, portMAX_DELAY);
    queueHead = 0; queueTail = 0; queueCount = 0;  
    xSemaphoreGive(queueMutex);  
}

uint16_t getQueueCount(){
    xSemaphoreTake(queueMutex, portMAX_DELAY);
    uint16_t count = queueCount;
    xSemaphoreGive(queueMutex);
    return count;
}

inline void appendToBuffer(uint8_t index, uint8_t* frame, uint8_t frameSize){
    memcpy(TXQueue[index] + TXQueueLens[index], frame, frameSize);
    TXQueueLens[index] += frameSize;
}
inline bool spaceInBuffer(uint8_t index, uint8_t frameSize){
    return TXQueueLens[index] + frameSize <= maxTXDataSize;
}

void addFrameToQueue(uint8_t* frame, uint8_t frameSize){
    xSemaphoreTake(queueMutex, portMAX_DELAY);
    //see if we can slot this packet into any not totally full packets already in queue
    for(int i = queueTail; i != queueHead; i = ((i+1) % maxPacketsInQueue)){   
        if(spaceInBuffer(i, frameSize)){
            appendToBuffer(i, frame, frameSize);
            xSemaphoreGive(queueMutex);
            return;
        }
    }
    //otherwise, slot it into a new spot
    if (queueCount == maxPacketsInQueue) {  //overide stale packet if needed
        queueTail = (queueTail + 1) % maxPacketsInQueue;
        queueCount--;
        logErr(queueOverflow);
    }
    //slot into new spot. start from the header, so we can update the header in place
    TXQueueLens[queueHead] = TXHeaderSize;
    appendToBuffer(queueHead, frame, frameSize);
    queueHead = (queueHead + 1) % maxPacketsInQueue;
    queueCount++;
    xSemaphoreGive(queueNonEmptyBinary);

    xSemaphoreGive(queueMutex);
}


//gives a new burst to use
void refreshBurstBuffer(uint8_t burstBuffer[maxPacketGroupSize][maxLoraPacketSize],
    uint8_t burstBufferLens[maxPacketGroupSize], uint8_t& burstBufferCount)
{
    if(queueCount == 0){
        //wait until something is put on the queue
        xSemaphoreTake(queueNonEmptyBinary, portMAX_DELAY);    
    }

    xSemaphoreTake(queueMutex, portMAX_DELAY);
    while (burstBufferCount < maxPacketGroupSize && queueCount > 0) {
        memcpy(&burstBuffer[burstBufferCount][TXHeaderSize], TXQueue[queueTail], maxTXDataSize);
        burstBufferLens[burstBufferCount] = TXQueueLens[queueTail];
        queueTail = (queueTail + 1) % maxPacketsInQueue;
        queueCount--;
        burstBufferCount++;
    }
    if(queueCount > 0){
        xSemaphoreGive(queueNonEmptyBinary);
    }
    xSemaphoreGive(queueMutex);
}

void waitForQueueItems(){

}

//insert a packet at start of burst buffer.
//Ideally called before refreshBurstBuffer
// void insertPriorityFrame(uint8_t burstBuffer[maxPacketGroupSize][maxLoraPacketSize], 
//     uint8_t burstBufferLens[maxPacketGroupSize], uint8_t* pFrame, uint8_t pFrameLen){
    
//     xSemaphoreTake(queueMutex, portMAX_DELAY);
    
// }