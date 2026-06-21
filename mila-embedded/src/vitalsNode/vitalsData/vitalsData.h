#ifndef vitalsData
#define vitalsData
#include "freertos/FreeRTOS.h"

#include "../../pecan/pecan.h"

#include "../vitalsGen/vitalsStructs.h"

#ifdef __cplusplus
extern "C" {
#endif

void vitalsDataInit(const UBaseType_t priority);
int16_t monitorData(CANPacket* message);

bool updateTelemetryDivider(CANFrame* frame, uint32_t newDivider);

bool requestEnableContactorsIfSafe(void);

#ifdef __cplusplus
}
#endif

#endif
