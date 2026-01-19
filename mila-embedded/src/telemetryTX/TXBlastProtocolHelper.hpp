#pragma once
#include <stdint.h>
#include <atomic>

//stuff from BlastProtocol.cpp that we want
extern SemaphoreHandle_t protocolMutex;
extern StaticSemaphore_t protocolMutexBuffer;
extern bool protocolRunning;
extern bool isBlasting;
extern std::atomic<bool> awaitingAck;
extern TimerHandle_t ackTimer;
extern uint32_t packetTimeOnAir_us;
extern bool ackParity;
//

bool nudgeTransmission();

void safeProtocolTransmit(const uint8_t* data, const uint16_t len, const uint32_t timeout_ms);

void processBitmap(uint16_t bitmap);

void sendNextPacketInBurst();

void startNewBurstSequence();

void listenForAck();

bool protocolGrab();

void protocolYield();
