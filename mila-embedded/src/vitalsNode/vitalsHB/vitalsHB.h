#ifndef vitalsHB
#define vitalsHB
#include "freertos/FreeRTOS.h"
#include "../../pecan/pecan.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

void HBInit(const UBaseType_t send_HB_Priority, const UBaseType_t check_HB_Priority) ;
int16_t receiveHeartbeat(CANPacket* message);

#ifdef __cplusplus
}
#endif

#endif
