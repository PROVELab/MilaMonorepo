#pragma once
#include <stdint.h>
#include <atomic>

#include "LoraTransmitQueue.hpp"

extern uint32_t packetTimeOnAir_us;
extern bool ackParity;
extern bool firstBurst;
extern TXQueue queue;
//

typedef enum{
    unstarted = 0,
    idle = 1,
    startNewBurst = 2,
    reloadBurst = 3,
    sendingData = 4,
    resendLastData = 5,
    awaitingAck = 6,
    crashed = 7,
} ProtocolState;

// void safeProtocolTransmit(const uint8_t* data, const uint16_t len, const uint32_t timeout_ms);

void waitForTransmitRequest();

ProtocolState prepareNewBurst();

ProtocolState sendData();

ProtocolState resendLastPacketInBurst();

ProtocolState awaitAck();