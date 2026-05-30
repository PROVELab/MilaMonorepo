
#include "LoraTransmitQueue.hpp"
#include "../LoraCommon/LoraErrLog.hpp"

static const char* TAG = "TX_Queue";

// --- queueBase Implementation ---
queueBase::queueBase(TX_Data_Packet* buffer, int size, SemaphoreHandle_t* signal) 
    : TXQueue(buffer), queueSize(size), wakeSignal(signal) {
    queueMutex = xSemaphoreCreateMutexStatic(&queueMutexBuffer);;
    assert(queueMutex != NULL);
}

void queueBase::increment(int& index) {
    index = (index == queueSize - 1) ? 0 : index + 1;
}

void queueBase::appendPayload(uint8_t index, uint8_t* payload, uint8_t payloadSize) {
    // The payload is appended to the .data member of the TXProtocolPacket.
    memcpy(TXQueue[index].packet.data + TXQueue[index].dataSize, payload, payloadSize);
    TXQueue[index].dataSize += payloadSize;
}

bool queueBase::spaceForPayload(uint8_t index, uint8_t payloadSize) {
    return TXQueue[index].dataSize + payloadSize <= maxTXDataSize;
}

bool queueBase::queueEmpty() { return queueCount == 0; }

bool queueBase::addFrameToBuffer(uint8_t* frame, uint8_t frameSize, bool overrideIfFull = true) {
    if(frameSize > maxTXDataSize) {return false;}
    xSemaphoreTake(queueMutex, portMAX_DELAY);
    uint64_t candidates = roomMask;
    // Scan all packets in the queue to check if they have room.
    while (candidates > 0) {
        // Get index of the first available packet
        int i = __builtin_ctzll(candidates); 
        
        if (spaceForPayload(i, frameSize)) {
            appendPayload(i, frame, frameSize);
            
            // If it's now too full for future frames, clear its bit
            if (maxTXDataSize - TXQueue[i].dataSize < MIN_USEFUL_SPACE) {
                roomMask &= ~(1ULL << i);
            }
            xSemaphoreGive(queueMutex);
            xSemaphoreGive(*wakeSignal);
            return true;
        }

        // This packet didn't fit this specific frame. clear it as an option.
        candidates &= ~(1ULL << i);
    }

    // Couldnt find any packets with space. Add a new packet
    if (queueCount == queueSize) {
        if(overrideIfFull == false){
            xSemaphoreGive(queueMutex);
            return false;
        }
        //otherwise, can overide last entry:
        TXQueue[queueTail].dataSize = 0;
        roomMask &= ~(1ULL << queueTail);
        increment(queueTail);
        queueCount--;
        logErr(TAG, queueOverflow); 
    }

    // Initialize the new packet at queueHead
    TXQueue[queueHead].dataSize = 0;
    appendPayload(queueHead, frame, frameSize);
    roomMask |= (1ULL << queueHead); //it can be used for future packets (almost certainly)
    increment(queueHead);
    queueCount++;
    
    xSemaphoreGive(queueMutex);
    xSemaphoreGive(*wakeSignal);
    return true;
}

bool queueBase::popFrame(TXProtocolPacket& packet, size_t& dataSize) {
    xSemaphoreTake(queueMutex, portMAX_DELAY);
    if(queueCount == 0){
        xSemaphoreGive(queueMutex);
        return false;   
    }
    //move it over
    dataSize = TXQueue[queueTail].dataSize;
    packet = TXQueue[queueTail].packet; // Struct copy
    //house keeping
    roomMask &= ~(1ULL << queueTail); //no longer a candidate
    increment(queueTail);
    queueCount--;
    xSemaphoreGive(queueMutex);
    return true;
}

// --- TXQueue Implementation ---
TXQueue::TXQueue() : 
    wakeSignal(xSemaphoreCreateBinary()), 
    queue(&wakeSignal), 
    priorityQueue(&wakeSignal) 
{
    assert(wakeSignal != NULL);
}

bool TXQueue::addFrameToQueue(uint8_t* frame, uint8_t frameSize) {
    const bool overrideIfFull = false;  //switch to true for  actual use later
    return queue.addFrameToBuffer(frame, frameSize, overrideIfFull);
}
bool TXQueue::addPriorityFrameToQueue(uint8_t* frame, uint8_t frameSize) {  
    const bool overrideIfFull = false; //priority queue should not override existing frames, to preserve priority order  
    return priorityQueue.addFrameToBuffer(frame, frameSize, overrideIfFull);
}

void TXQueue::insertErrorFrame(TXProtocolPacket burstBuffer[maxPacketGroupSize], size_t burstPacketSizes[maxPacketGroupSize], uint8_t& numPackets){
    // 1. Get pending errors. This also clears the error log.
    uint8_t errPayload[(maxErrorCount * sizeof(Error_Type)) + 1];

    uint8_t errCount = getErrorPacket((int16_t*)(errPayload + 1));
    if(errCount == 0) return; //no errors to log

    errPayload[0] = ((errCount & 0x0F) << 4) | (vitalsErr & 0x0F);
    const uint8_t payloadSize = 1 + (errCount * sizeof(Error_Type));

    // 3. Try to add to the priority queue without overriding if it's full.
    if(priorityQueue.addFrameToBuffer(errPayload, payloadSize, false)){
        return;
    }

    // 4. Fallback: Manually insert into the current burst if there's space.
    if(numPackets >= maxPacketGroupSize) {
        return;
    }
    // Copy the error payload into the data section of the TXProtocolPacket
    memcpy(burstBuffer[numPackets].data, errPayload, payloadSize);
    burstPacketSizes[numPackets] = payloadSize + TXHeaderSize;
    // Safely set the flags directly on the struct
    burstBuffer[numPackets].flags = txFlagMasks::priorityPacketMask;
    numPackets++;
}

//returns size of the burstBuffer. not thread safe (only call from one spot..)
uint8_t TXQueue::refreshBurstBuffer(TXProtocolPacket burstBuffer[maxPacketGroupSize], size_t burstPacketSizes[maxPacketGroupSize], uint8_t startIndex) {
    uint8_t numPackets = startIndex;
    size_t payloadSize = 0;
    insertErrorFrame(burstBuffer, burstPacketSizes, numPackets);
    
    do{
        while(numPackets < maxPacketGroupSize && 
            priorityQueue.popFrame(burstBuffer[numPackets], payloadSize) )
        {
            burstPacketSizes[numPackets] = payloadSize + TXHeaderSize;
            burstBuffer[numPackets].flags |= txFlagMasks::priorityPacketMask;
            numPackets++;
        }
        while(numPackets < maxPacketGroupSize && 
            queue.popFrame(burstBuffer[numPackets], payloadSize) )
        {
            burstPacketSizes[numPackets] = payloadSize + TXHeaderSize;
            burstBuffer[numPackets].flags &= ~txFlagMasks::priorityPacketMask;
            numPackets++;
        }   
        if(numPackets == 0){
            xSemaphoreTake(wakeSignal, portMAX_DELAY);  //wait for a packet
        }
    }  while(numPackets == 0); 

    return numPackets;
}
