#include "Config.hpp"

#ifndef LORA_DRIVER_H
#define LORA_DRIVER_H

#include <stdint.h>
#include <stddef.h>

#define crashMsgSize 80

enum driverState{
    off = 0,    //starting state, or afer crash
    standby = 1,
    running = 2
};

//make a binary this task gives, so protocol recv irq & if in crash state
struct driverInfo{
    //relevant if driver crashes
    driverState state; //current state of driver, mainly for protocol to decide what to do. Set to crash if we had a crash, and protocol should check crashError and crashMsg for details
    //relevant if state == off
    int16_t crashError;
    char crashMsg[crashMsgSize];
    //

    //for recv messages. can check
    bool recvPacketReady;

    driverRecvPacket recvPacket; //set on interrupt, protocol can read when it gets the driver mutex
};

// #ifdef __cplusplus
// extern "C" {
// #endif

// ******** Init ***********//
void LoraDriverInit(const RadioConfig* config);
// ^^If driver cant proceed, LoraCrashHandle is notified with param = a RadioLibError (int16_r)
// call this function again to restart, only safe to call if driver is off.

//for use by protocol if it wants to trigger a crash. Will put driver into off state.
void raiseDriverCrash(int16_t error, const char* msg);


// Initiate RX/TX transmission, call the protocol callback when complete
//can disregard LoraTransmitErrors. They should be propogated to crash, might give some extra info tho.
int16_t LoraTransmit(const driverSendPacket* packet, const uint64_t timerExpireTime_us);
void LoraStartRecv();   //use safeWaitForRecv, if you want a timeout and error checking
//user should check info if crashed is true
driverInfo* waitForRecv(uint64_t timerExpireTime_us);    //extension of LoraStartRecv with timeout
uint32_t LoraGetTimeOnAir();
//
//user should check crashed if returns false
bool waitForTXDone(uint8_t numPacketTimes);

void enterStandBy();
driverInfo* waitForDriverAction(uint32_t timeoutDuration_us);
int16_t waitIfReceiving(uint64_t timerExpireTime_us);   //LoraTransmit helper. Wait for current RX to finish if we are currently receiving. Returns error if we had an issue waiting, or if we werent receiving by the time we timed out. Returns RADIOLIB_ERR_NONE if we successfully waited for a receive to finish, or if we werent receiving by the time we called this function.
driverInfo* getDriverInfo();
// #ifdef __cplusplus
// }
// #endif

#endif
