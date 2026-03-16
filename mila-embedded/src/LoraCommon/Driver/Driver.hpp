#include "Config.hpp"

#ifndef LORA_DRIVER_H
#define LORA_DRIVER_H

#include <stdint.h>
#include <stddef.h>

#define crashMsgSize 80

enum driverState{
    off = 0,    //starting state
    standby = 1,
    running = 2
};

//make a binary this task gives, so protocol recv irq & if in crash state
struct driverInfo{
    //relevant if driver crashes
    driverState state; //current state of driver, mainly for protocol to decide what to do. Set to crash if we had a crash, and protocol should check crashError and crashMsg for details
    bool driverRunning;
    int16_t crashError;
    char crashMsg[crashMsgSize];
    //

    //for recv messages. can check
    bool recvPacketReady;
    driverPacket recvPacket; //set on interrupt, protocol can read when it gets the driver mutex
};

// #ifdef __cplusplus
// extern "C" {
// #endif

// ******** Init ***********//
void LoraDriverInit(const RadioConfig* config);
// ^^If driver cant proceed, LoraCrashHandle is notified with param = a RadioLibError (int16_r)
//Driver suspends. Dont try to resume. Instead, call driver reboot to restart the task.
void LoraDriverRestart(const RadioConfig* config);

// ******* Start RX/TX ******* ///
// Initiate RX/TX transmission, call the protocol callback when complete
//can disregard LoraTransmitErrors. They should be propogated to crash, might give some extra info tho.
int16_t LoraTransmit(const driverPacket* packet, const uint64_t timerExpireTime_us);
void LoraStartRecv();
driverInfo* safeWaitForRecv(uint64_t timerExpireTime_us);    //extension of LoraStartRecv with timeout
uint32_t LoraGetTimeOnAir();
//

void enterStandBy();
driverInfo* waitForDriverAction(uint32_t timeoutDuration_ms);
int16_t waitIfReceiving(uint64_t timerExpireTime_us);   //LoraTransmit helper. Wait for current RX to finish if we are currently receiving. Returns error if we had an issue waiting, or if we werent receiving by the time we timed out. Returns RADIOLIB_ERR_NONE if we successfully waited for a receive to finish, or if we werent receiving by the time we called this function.
driverInfo* getDriverInfo();
// #ifdef __cplusplus
// }
// #endif

#endif
