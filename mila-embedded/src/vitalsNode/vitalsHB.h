#ifndef vitalsHB
#define vitalsHB
#include "../pecan/pecan.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

int16_t recieveHeartbeat(CANPacket* message);
void sendHB(void* pvParameters);

#ifdef __cplusplus
}
#endif

#endif
