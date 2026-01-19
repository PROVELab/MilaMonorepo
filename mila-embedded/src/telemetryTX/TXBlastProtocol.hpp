#pragma once
#include <stdint.h>
#include "LoraErrLog.hpp"
#include "../LoraCommon/LoraDriverConfig.hpp"   //RadioConfig

void initProtocol(const RadioConfig config);

extern SemaphoreHandle_t crashBinary; //notify API to return error to user
extern int16_t driverCrashError;                //values returned to user
extern char driverCrashMsg [crashMsgSize];

bool protocolTransmit(uint8_t* data, uint8_t dataLen);

//not implementing for now
//bool protocolTransmitPriority(uint8_t* data, uint8_t dataLen);
