#ifndef ERROR_STRUCT_H
#define ERROR_STRUCT_H

//struct used on both TX and RX side for logging errors
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
    timerStartErr          = 6,
    unexpectedTimeoutFire  = 7,
    ackTimeout             = 8,
    invalidRXLength        = 9,
    queueOverflow          = 10,
    burstFillFailure       = 11,
    driverNotStarted     = 12
    //Raised by Vitals non-Lora code

} custom_Vitals_Err_Codes; 

#endif  // ERROR_STRUCT_H