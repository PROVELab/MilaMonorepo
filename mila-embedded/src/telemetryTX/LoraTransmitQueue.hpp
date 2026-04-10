#ifndef TX_QUEUE_HPP
#define TX_QUEUE_HPP

#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "../LoraCommon/blastProtocolConfig.hpp"

#define priorityTX_queueSize 4
#define TX_queueSize 64

struct TX_Data_Packet {
    size_t dataSize;
    uint8_t data[maxTXDataSize];
};

// --- BASE LOGIC (Compiled once in .cpp) ---
class queueBase {
protected:
    SemaphoreHandle_t queueMutex = NULL;
    TX_Data_Packet* TXQueue;
    int queueSize;
    int queueHead = 0, queueTail = 0, queueCount = 0;
    uint64_t roomMask = 0;
    static constexpr uint8_t MIN_USEFUL_SPACE = 5;
    SemaphoreHandle_t* wakeSignal;

    queueBase(TX_Data_Packet* buffer, int size, SemaphoreHandle_t* signal);

    void appendToBuffer(uint8_t index, uint8_t* frame, uint8_t frameSize);
    bool spaceInBuffer(uint8_t index, uint8_t frameSize);
    void increment(int& index);
    bool insertAtFrontIfSpace(uint8_t* frame, uint8_t frameSize, uint8_t* packetsToCheck);

public:
    bool queueEmpty();
    bool addFrameToBuffer(uint8_t* frame, uint8_t frameSize, bool overrideIfFull);
    bool popFrame(uint8_t* newLocation, size_t& frameSize);
};

template <uint16_t static_queueSize>
class staticQueue : public queueBase {
    static_assert(static_queueSize <= 64, "Mask only supports up to 64 slots");
private:
    TX_Data_Packet queue[static_queueSize];
public:
    staticQueue(SemaphoreHandle_t* wakeSignal) 
        : queueBase(queue, static_queueSize, wakeSignal) {}
};

class TXQueue {
private: 
    SemaphoreHandle_t wakeSignal;
    staticQueue<TX_queueSize> queue;
    staticQueue<priorityTX_queueSize> priorityQueue;
public:
    TXQueue();
    bool addFrameToQueue(uint8_t* frame, uint8_t frameSize);
    bool addPriorityFrameToQueue(uint8_t* frame, uint8_t frameSize);
    uint8_t refreshBurstBuffer(driverSendPacket burstBuffer[maxPacketGroupSize], uint8_t startIndex = 0);
private: 
    void insertErrorFrame(driverSendPacket burstBuffer[maxPacketGroupSize], uint8_t& numPackets);
};

#endif