#ifndef VITALS_CONTACTOR_CONTROL_H
#define VITALS_CONTACTOR_CONTROL_H


#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "../programConstants.h"


typedef enum {
    enableContactors = 0,
    disableContactors = 1, //not used atm. may be trigged by vitals down the line if we want non-latching errors
    interModuleContactorsEnabled = 2,
    prechargeContactorsEnabled = 3,
} vitalsContactorCommands;

void contactorControlInit();

void sendContactorControlCommand(vitalsContactorCommands command);    //may be called from anywhere in program

bool enableContactorsIfSafe();

void getContactorState(vitalsContactorState* state); 


#ifdef __cplusplus
}
#endif

#endif // VITALS_CONTACTOR_CONTROL_H
