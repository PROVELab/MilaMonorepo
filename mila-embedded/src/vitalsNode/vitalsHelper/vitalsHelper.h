#ifndef vitalsHelp
#define vitalsHelp

#include <stdatomic.h>

#include "../../programConstants.h"
#include "../vitalsGen/vitalsStructs.h"

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

// helpers for atomic use of modifiable fields
static inline void VitalsFlagSet(uint8_t nodeIndex, uint32_t bit) { atomic_fetch_or(&(nodes[nodeIndex].flags), bit); }
static inline void VitalsFlagClear(uint8_t nodeIndex, uint32_t bit) { atomic_fetch_and(&(nodes[nodeIndex].flags), ~bit); }
static inline uint8_t VitalsFlagsGet(uint8_t nodeIndex) { return atomic_load(&(nodes[nodeIndex].flags)); }

static inline void HBTimeSet(uint8_t nodeIndex, uint16_t time) { atomic_store(&(nodes[nodeIndex].milliSeconds), time); }
static inline int16_t HBTimeGet(uint8_t nodeIndex) { return atomic_load(&(nodes[nodeIndex].milliSeconds)); }
//

//forwarding
int16_t forwardStatusUpdate(CANPacket* message);
int16_t vitals_defaultPacketRecv(CANPacket* p);

#endif