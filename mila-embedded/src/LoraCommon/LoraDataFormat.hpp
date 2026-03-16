// #pragma once
// #include "../pecan/pecan.h"
// #include "Driver/Config.hpp"   //RadioConfig, getStandardConfig

// //Calls Lora protocol functions to recv/send, and formats 
// #ifdef __cplusplus
// extern "C" { //Need C linkage since ESP uses C "C"
// #endif

// //** thread safe  **//
// //queue messages for transmission. Return true if successfully queued
// //checks that packetdataLen <=8. pecan CANPacket arrays are 8 bytes!
// bool Lora_API_TransmitCANFrame(CANPacket* packet, bool highPriority);

// //same as TransmitCANFrame, but dataLen can be up to maxCanToLoraData, since it does not use CANPacket
// #define maxCanToLoraData 15 //if increase, need to increase how many bits are used for dataLen in theheader
// bool Lora_API_TransmitOversizeFrame(uint32_t id, uint8_t* data, int8_t dataLen, bool highPriority);
// //**  **//

// #ifdef __cplusplus
// }  // End extern "C"
// #endif