#pragma once 
#include <stdint.h>

#include "blastProtocolConfig.hpp"

//error codes used on TX side by telem/vitals for logging errors
//Positive = Our error Codes. negative = RadioLib error codes

using Error_Type = int16_t; //each error is a 16 bit int

using custom_Err_Code_Flags = uint32_t;  //can make this bigger if below enum reaches value > 32

#define maxErrorCount 16

static_assert(maxErrorCount * sizeof(Error_Type) <= maxTXDataSize, "maxErrorCount too high to fit in Lora msg");

//used on TX size to ensure we dont exceed max bits of custom err flags
#define maxErr_Code_Num (sizeof(custom_Err_Code_Flags) * 8) - 1

typedef enum {
    //Lora Specific:
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
    burstFillFailure       = 12,
    RX_TIMEOUT             = 13,
    TX_Queue_Msg_Timeout   = 14,    //very common on timeout
    TX_Completion_Timeout  = 15,     //shouldnt be possible?
    TX_Completion_Crash    = 16,
    driverOff              = 17,
    recvUnknownSensorID    = 18,

    //Vitals Specific:
    //Raised by Vitals non-Lora code

} custom_Vitals_Er;

void initErr();

//Can be called by Lora Driver or Vitals.
//Positive = Our error Codes. negative = RadioLib error codes
void logErr(const char* TAG, int16_t err);

//only for TX side atm.
uint8_t getErrorPacket(int16_t* errPacket);
