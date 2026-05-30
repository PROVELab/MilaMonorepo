#include <stdint.h>

#include "../../pecan/pecan.h"
#include "vitalsHelper.h"
#include "vitalsPacketSendLUT.h"
#include "../../programConstants.h"

#if defined(__cplusplus)
#define STATIC_ASSERT(cond, msg) static_assert((cond), msg)
#else
#define STATIC_ASSERT(cond, msg) _Static_assert((cond), msg)
#endif

static inline uint32_t mask_u32(unsigned bits) { return (bits >= 32) ? 0xFFFFFFFFu : ((1u << bits) - 1u); }

// Must ensure the ID/index is valid before calling either of these.
int32_t IDTovitalsIndex(uint32_t nodeID) { // returns which index of vitalsArray a node corresponds to
    for(int i =0; i < numberOfNodes; i++){
        if(nodes[i].numFrames > 0 && nodes[i].CANFrames->nodeID == nodeID){
            return i;
        }
    }
    //otherwise compute based on missing IDs
    uint32_t baseID = getNodeId(nodeID);
    // loop over excluded
    int16_t foundMisses = 0;
    for (int i = 0; missingIDs[i] < baseID && foundMisses < numMissingIDs; i++) { foundMisses++; }
    // Example: if we get 11, with base 6, two missing IDs, and 3 nodes
    // (id must be 6-10): 11 - 6 - 2 = 3. totalNumNodes =3, since >=, invalild
    int32_t nodeIndex = baseID - startingOffset - foundMisses;
    if (nodeIndex >= numberOfNodes || nodeIndex < 0) { return invalidVitalsIndex; }
    return nodeIndex;
}

uint32_t vitalsIndexToID(uint32_t nodeIndex) { // inverse of above function
    // CAN Frames have to store nodeID anyway. can use this as a shortcut
    if (nodes[nodeIndex].numFrames > 0) { return nodes[nodeIndex].CANFrames->nodeID; }
    // Otherwise compute based on missingID's
    uint32_t baseID = nodeIndex + startingOffset;
    // loop over excluded
    int16_t foundMisses = 0;
    for (int i = 0; missingIDs[i] <= baseID && foundMisses < numMissingIDs; i++) {
        baseID++;
        foundMisses++;
    }
    return baseID;
}

int32_t isolateBits(uint8_t* value, int8_t startingIndex, int8_t numBits) {
    if (value == NULL || startingIndex < 0 || numBits <= 0 || numBits > 32 || startingIndex + numBits > 64) {
        return 0;
    }
    const uint64_t data = *((uint64_t*) value);
    const uint64_t mask = ((uint64_t) 1u << numBits) - 1u; // create numBits-wide mask
    return (int32_t) ((data >> startingIndex) & mask);
}
