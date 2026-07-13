#include <stdint.h>
#include <inttypes.h>
#include "esp_log.h"

#include "../../pecan/pecan.h"
#include "vitalsHelper.h"
#include "../vitalsGen/vitalsStructs.h"
#include "../../programConstants.h"

static const char* TAG = "vitalsHelper";

// Must ensure the ID/index is valid before calling either of these.
int32_t IDTovitalsIndex(uint32_t nodeID) { // returns which index of vitalsArray a node corresponds to
    for(int i =0; i < numberOfNodes; i++){
        if(nodes[i].CAN_ID == nodeID) { //find the matching CAN_ID
            return i;   //return the index vitals stores the node with this CAN_ID
        }
    }
    return invalidVitalsIndex;
}

uint32_t vitalsIndexToID(uint32_t nodeIndex) { // inverse of above function
    if(nodeIndex < numberOfNodes){
        return nodes[nodeIndex].CAN_ID;
    }
    return invalidVitalsIndex;
}

bool vitalsGlobalFrameIDToLocal(const CANFrame* frame, int32_t* localFrameID) {

    vitalsNode* node = &nodes[frame->nodeIndex];

    const int32_t startFrameID = node->CANFrames[0].frameID;
    *localFrameID = (int32_t)frame->frameID - startFrameID;
    if (*localFrameID < 0 || *localFrameID >= node->numFrames) {
        ESP_LOGE(TAG,
                 "Invalid local frame ID %" PRIi32 " for node %" PRIi8
                 " (global frameID=%" PRIi8 ", startFrameID=%" PRIi32 ", numFrames=%" PRIi8 ")",
                 *localFrameID, node->CAN_ID, frame->frameID, startFrameID, node->numFrames);
        return false;
    }

    return true;
}
