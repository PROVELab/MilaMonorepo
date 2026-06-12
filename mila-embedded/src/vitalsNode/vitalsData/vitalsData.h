#ifndef vitalsData
#define vitalsData
#include "../../pecan/pecan.h"

#ifdef __cplusplus
extern "C" {
#endif

void initializeVitalsData();
int16_t monitorData(CANPacket* message);

#ifdef __cplusplus
}
#endif

#endif
