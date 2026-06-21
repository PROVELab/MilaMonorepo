#ifndef INTERMODULE_CONTACTOR_H
#define INTERMODULE_CONTACTOR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
#include "../../pecan/pecan.h"

int16_t handleVitalsCommand(CANPacket* p);
void registerVitalsContactorHandler(PCANListenParamsCollection* plpc);


#ifdef __cplusplus
}
#endif

#endif // INTERMODULE_CONTACTOR_H
