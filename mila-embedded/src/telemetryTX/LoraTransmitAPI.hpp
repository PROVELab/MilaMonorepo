#pragma once
#include "../pecan/pecan.h"
#include "../LoraCommon/LoraDriverConfig.hpp"   //RadioConfig

#ifdef __cplusplus
extern "C" { //Need C linkage since ESP uses C "C"
#endif
void Lora_TX_Init(const RadioConfig config);
void Lora_TX_Restart(const RadioConfig config);

bool protocolTransmitCANFrame(CANPacket* packet);

//same as transmitCANFrame, but dataLen can be up to 15, and just pass in data + id, no need to construct CANPacket
//intended for use with sending msg to telem that isnt directly a Can Packet
void protocolTransmitOversizeFrame(uint32_t id, uint8_t* data, int8_t dataLen);

int16_t Lora_Monitor_Crash(char*& errorMsg, uint8_t& msgLen);

#ifdef __cplusplus
}  // End extern "C"
#endif