#include <stdint.h>
#include <string.h>

#include "freertos/FreeRTOS.h"

#include "../LoraCommon/blastProtocolConfig.hpp"
#include "LoraErrLog.hpp"

static uint8_t TXQueue[maxPacketsInQueue][protocolPacketDataBytes];
static uint8_t TXQueueLens[maxPacketsInQueue];

static uint16_t queueHead = 0, queueTail = 0, queueCount = 0;

static SemaphoreHandle_t queueMutex = NULL; //mutex to take or add to msg queue.

//may be called again on restart. Guaranteed not to have other queue functions running when called.
void initQueue(){
    if(queueMutex == NULL){ 
        queueMutex = xSemaphoreCreateMutex();
    }
    xSemaphoreTake(queueMutex, portMAX_DELAY);
    queueHead = 0; queueTail = 0; queueCount = 0;  
    xSemaphoreGive(queueMutex);  
}

inline void appendToBuffer(uint8_t index, uint8_t* frame, uint8_t frameSize){
    memcpy(TXQueue[index] + TXQueueLens[index], frame, frameSize);
    TXQueueLens[index] += frameSize;
}
inline bool spaceInBuffer(uint8_t index, uint8_t frameSize){
    return TXQueueLens[index] + frameSize <= protocolPacketDataBytes;
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
    //slot into new spot
    TXQueueLens[queueHead] = headerSize;
    appendToBuffer(queueHead, frame, frameSize);
    queueHead = (queueHead + 1) % maxPacketsInQueue;
    queueCount++;

    xSemaphoreGive(queueMutex);
}

//returns number of packets in the refreshed Burst
void refreshBurstBuffer(uint8_t burstBuffer[maxPacketGroupSize][maxLoraPacketSize],
    uint8_t burstBufferLens[maxPacketGroupSize], uint8_t& burstBufferCount)
{
    xSemaphoreTake(queueMutex, portMAX_DELAY);
    while (burstBufferCount < maxPacketGroupSize && queueCount > 0) {
        memcpy(&burstBuffer[burstBufferCount][headerSize], TXQueue[queueTail], protocolPacketDataBytes);
        burstBufferLens[burstBufferCount] = TXQueueLens[queueTail];
        queueTail = (queueTail + 1) % maxPacketsInQueue;
        queueCount--;
        burstBufferCount++;
    }
    xSemaphoreGive(queueMutex);
}

//insert a packet at start of burst buffer.
//Ideally called before refreshBurstBuffer
void insertPriorityFrame(uint8_t burstBuffer[maxPacketGroupSize][maxLoraPacketSize], 
    uint8_t burstBufferLens[maxPacketGroupSize], uint8_t* pFrame, uint8_t pFrameLen){
    
    xSemaphoreTake(queueMutex, portMAX_DELAY);
    
}