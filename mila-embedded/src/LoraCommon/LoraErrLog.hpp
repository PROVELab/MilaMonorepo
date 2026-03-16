#pragma once 
#include <stdint.h>

#ifdef __cplusplus
extern "C" { //Need C linkage since ESP uses C "C"
#endif


//error codes used on TX side by telem/vitals for logging errors
//Positive = Our error Codes. negative = RadioLib error codes

#define maxRadioErrs 8      //can log at most 8 errors per burst
using Error_Type = int16_t; //each error is a 16 bit int

using custom_Err_Code_Flags = uint32_t;  //can make this bigger if below enum reaches value > 32

//used on TX size to ensure we dont exceed max bits of custom err flags
#define maxErr_Code_Num (sizeof(custom_Err_Code_Flags) * 8) - 1

typedef enum {
    //Lora Specific:
    RX_BUSY_TIMEOUT = 1,
    AIR_ACTIVITY_TIMEOUT = 2,
    unexpectedTXCompletion = 3,
    unexpectedRXCompletion = 4,
    incorrectProtocolId    = 5,
    recvOutdatedAck        = 6,
    timerStartErr          = 7,
    unexpectedTimeoutFire  = 8,
    ackTimeout             = 9,
    invalidRXLength        = 10,
    queueOverflow          = 11,
    burstFillFailure       = 12
    //Raised by Vitals non-Lora code

} custom_Vitals_Er;

void initErr();

//Can be called by Lora Driver or Vitals.
//Positive = Our error Codes. negative = RadioLib error codes
void logErr(int16_t err);

#ifdef __cplusplus
}  // End extern "C"
#endif