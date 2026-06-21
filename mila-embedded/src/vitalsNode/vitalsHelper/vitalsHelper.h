#ifndef vitalsHelp
#define vitalsHelp

#include <stdatomic.h>

#include "../../programConstants.h"
#include "../vitalsGen/vitalsStructs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define invalidVitalsIndex -1 

// convert between can id's and vitals LUT indices
int32_t IDTovitalsIndex(uint32_t nodeID);
uint32_t vitalsIndexToID(uint32_t nodeIndex); // inverse of above
bool vitalsGlobalFrameIDToLocal(const CANFrame* frame, int32_t* localFrameID);

#ifdef __cplusplus
}
#endif

#endif
