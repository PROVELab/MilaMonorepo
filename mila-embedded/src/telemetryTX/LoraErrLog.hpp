#pragma once 
#include <stdint.h>
#include "../LoraCommon/ErrorStruct.h"  //custom_Vitals_Err_Codes

#ifdef __cplusplus
extern "C" { //Need C linkage since ESP uses C "C"
#endif

//For logging when driver Crashes (gives up)
#define crashMsgSize 40
void logCrash(const int16_t error, const char* msg);

void initErr();

//Can be called by Lora Driver or Vitals.
//Positive = Our error Codes. negative = RadioLib error codes
void logErr(int16_t err);

#ifdef __cplusplus
}  // End extern "C"
#endif