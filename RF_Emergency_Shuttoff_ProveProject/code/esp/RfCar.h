#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "pecan/pecan.h"
#ifdef __cplusplus
extern "C" {
#endif
void rfCarInit(void);
void rfCarRegisterCan(PCANListenParamsCollection *listeners);
bool rfCarPermit(void);
/* No dashboard reset hook: connect ONLY to an approved deliberate local reset.
 * off_confirmed must come from verified physical state, not allOff software state. */
bool rfCarLocalReset(bool deliberate, bool off_confirmed);
#ifdef __cplusplus
}
#endif
