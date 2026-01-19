#ifndef LORA_DRIVER_H
#define LORA_DRIVER_H

#include <stdint.h>
#include <stddef.h>

#include "LoraDriverConfig.hpp"

#ifdef __cplusplus
extern "C" {
#endif

// ******** Init ***********//
void LoraDriverInit(const RadioConfig config);
// ^^If driver cant proceed, LoraCrashHandle is notified with param = a RadioLibError (int16_r)
//Driver suspends. Dont try to resume. Instead, call driver reboot to restart the task.
void LoraDriverRestart(const RadioConfig config);

// ******* Start RX/TX ******* ///
// Initiate RX/TX transmission, call the protocol callback when complete
//Errors are most likely an issue with SPI or chip, or if air is never quite for TX to begin
int16_t LoraTransmit(const uint8_t* data, const uint16_t dataLen, const uint64_t timerExpireTime_us);
void LoraStartRecv();
uint32_t LoraGetTimeOnAir();
//

//Callbacks for protocol when RX/TX finish. Protocol must implement these
void protocolTXComplete();
void protocolReceive(const uint8_t* rxBuffer, const size_t length);
//Callback for when driver Crashes. Protocol must implement these
void protocolCrash(const int16_t error, const char* msg);    //function to be called by driver to crash

#ifdef __cplusplus
}
#endif

#endif
