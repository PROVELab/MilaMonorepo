#ifndef vitalsHelp
#define vitalsHelp

#ifdef __cplusplus
#include <atomic>
#endif

#include "../../programConstants.h"
#include "vitalsStaticDec.h"
#include "vitalsStructs.h"

#ifdef __cplusplus
extern "C" {
#endif

// fixed vitals Constants
enum internalVitalsFlags { // flags being placed in Vitals' flags field for each node
    HBFlag = 1
};
#define invalidVitalsIndex -1

// returns which index of vitalsArray a node corresponds to
int32_t IDTovitalsIndex(uint32_t nodeID);
uint32_t vitalsIndexToID(uint32_t nodeIndex); // inverse of above

// return an integer containing numBits  of value starting at startingIndex (bit-Indexed).
//  EX: (0b10101, 1, 3) -> 0b010
int32_t isolateBits(uint8_t* value, int8_t startingIndex, int8_t numBits);

// Helpers for sending warnings.
void sendWarningForDataPoint(const CANFrame* problemFrame, uint8_t dataPointIndex, uint32_t flags);
void sendWarningForNode(uint8_t nodeID, uint32_t flags);

#ifdef __cplusplus
} // extern "C"

// helpers for atomic use of modifiable fields
inline void VitalsFlagSet(uint8_t nodeIndex, uint32_t bit) { nodes[nodeIndex].flags.fetch_or(bit); }
inline void VitalsFlagClear(uint8_t nodeIndex, uint32_t bit) { nodes[nodeIndex].flags.fetch_and(~bit); }
inline uint8_t VitalsFlagsGet(uint8_t nodeIndex) { return nodes[nodeIndex].flags.load(); }

inline void HBTimeSet(uint8_t nodeIndex, uint16_t time) { nodes[nodeIndex].milliSeconds.store(time); }
inline int16_t HBTimeGet(uint8_t nodeIndex) { return nodes[nodeIndex].milliSeconds.load(); }
//
#endif

#else // Not C++
#error "This project must be compiled as C++"
#endif // __cplusplus

#endif // vitalsHelp
