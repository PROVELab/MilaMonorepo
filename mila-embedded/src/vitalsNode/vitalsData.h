#ifndef vitalsData
#define vitalsData
#include "../pecan/pecan.h"

#ifdef __cplusplus
extern "C" {
#endif

int16_t monitorData(CANPacket* message);
void initializeDataTimers();

#ifdef __cplusplus
}
#endif

#endif
