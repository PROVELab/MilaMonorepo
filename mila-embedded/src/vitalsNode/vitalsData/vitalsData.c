#include <inttypes.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "../../pecan/pecan.h"

#include "vitalsDataHelper.h"
#include "../../programConstants.h"
#include "../vitalsHelper/vitalsHelper.h"
#include "../vitalsGen/vitalsPacketSendLUT.h"
#include "../vitalsGen/vitalsStructs.h"

static const char* TAG = "VitalsData";

//frame timeouts
TimerHandle_t missingDataTimers[totalNumFrames];  // trigers callback function for missing CAN Data Frane
StaticTimer_t xTimerBuffers[totalNumFrames];      // array for the buffers of these timers

static void initializeDataTimers();
void vitalsDataInit(const UBaseType_t priority){ // initializes timeOuts for Data collection, as soon as this runs, we need data from every
                              // node to be sending their data to prevent them getting flagged, or Bus off if critical
    initializeDataModifier(priority);
    initializeDataTimers();
}

bool requestEnableContactorsIfSafe(void) {
    DataModifierEvent event = {
        .eventType = ATTEMPT_ENABLE_CONTACTORS,
    };

    if (xQueueSend(dataModifierQueue, &event, 0) != pdPASS) {
        ESP_LOGW(TAG, "Failed to queue contactor enable safety request.");
        return false;
    }

    return true;
}

bool updateTelemetryDivider(CANFrame* frame, uint32_t newDivider){
    telemDividerRequest request = {
        .frame = frame,
        .newDivider = newDivider
    };
    DataModifierEvent event = {
        .eventType = UPDATE_TELEMETRY_DIVIDER,
        .payload.dividerRequest = request
    };

    if (xQueueSend(dataModifierQueue, &event, 0) != pdPASS) {
        ESP_LOGW(TAG, "Failed to update telemetry divider for %" PRIu32, newDivider);
        return false;
    }
    return true;
}

static void initializeDataTimers(){
    int32_t numInits = 0;
    ESP_LOGI(TAG, "Initializing data timers...");
    for (int i = 0; i < numberOfNodes; i++) {
        for (int j = 0; j < nodes[i].numFrames; j++) {
            missingDataTimers[numInits] =
                xTimerCreateStatic(/* Just a text name, not used by the RTOS kernel. */
                                   "Timer",
                                   /* The timer period in ticks, must be greater than 0. */
                                   pdMS_TO_TICKS(nodes[i].CANFrames[j].dataTimeout),
                                   /* The timers will auto-reload themselves when they expire. */
                                   pdTRUE,
                                   /* pointer to the canFrame to identify which frame is missing */
                                   (void*) &(nodes[i].CANFrames[j]),
                                   vTimerCallback,            // the callback function (same for all timers)
                                   &(xTimerBuffers[numInits]) // buffer that holds timer info stuff
                );
            if (missingDataTimers[numInits] == NULL) {
                ESP_LOGE(TAG, "Error creating timer, aborting");
                while (1);
            }
            numInits++;
        }
    }
    // start the timers:
    for (int i = 0; i < numInits; i++) {
        if (xTimerStart(missingDataTimers[i], pdMS_TO_TICKS(1000)) ==
            pdFAIL) { // no time crunch yet, but if this isnt starting we want to be notified, instead of it to running
                      // forever
            ESP_LOGE(TAG, "Unable to start a timer");
            while (1);
        }
    }
    ESP_LOGI(TAG, "Number of data timers initialized: %" PRIi32, numInits);
}

 // for now just stores the data (printing the past 10 node-frame- data (past 10) on each line)
int16_t monitorData(CANPacket* message) {
    const uint32_t CAN_node_ID = getNodeId((uint32_t)(message->id));
    const int16_t nodeIndex = IDTovitalsIndex(CAN_node_ID);
    if (nodeIndex == invalidVitalsIndex) { // Note: message->id is int32_t
        ESP_LOGW(TAG, "Received data from invalid nodeId %" PRIu32 " (raw CAN id %" PRIi32 "), ignoring", CAN_node_ID, message->id);
        return 1;
    }

    vitalsNode* node = &(nodes[nodeIndex]); // the node which sent the message

    uint32_t CanFrameNumber = getDataFrameId(message->id); // the Can frame index is stored in extension
    if (CanFrameNumber >= node->numFrames) {
        ESP_LOGW(TAG, "Invalid dataFrame %" PRIu32 " for node %" PRIu32 ". Ignoring data", CanFrameNumber, CAN_node_ID);
        return 1;
    }
    ESP_LOGD(TAG, "Received data for node %" PRIu32 ", frame %" PRIu32, CAN_node_ID, CanFrameNumber);
    CANFrame* frame = &(node->CANFrames[CanFrameNumber]); // the frame this data corresponds to
    // mark this data as collected.

    if (xTimerReset(missingDataTimers[frame->frameID], pdMS_TO_TICKS(100)) == pdFAIL) { // wait up to 100ms to reset timer
        sendframeWarningArgs args = {.mask = 0,
                                     .frameErrorTrigger = {.e = dataTimeout},
                                     .nodeID = (int32_t)node->CAN_ID,
                                     .frameID = (int32_t)frame->frameID};
        sendframeWarningFunction(args);

        ESP_LOGW(TAG, "Unable to reset data timer for node %" PRIu32 " frame %" PRIu32, CAN_node_ID, CanFrameNumber);
    }

    // Send every frame through dataModifier. It now handles both:
    // - critical datapoints with history/outlier/extrapolation logic
    // - non-critical datapoints for normal forwarding/packing

    DataModifierEvent addEvent = {
        .eventType = ADD_FRAME,
        .payload.request = {
            .frame = frame,
            .canFrameNumber = CanFrameNumber,
            .packet = *message,
        }
    };
    
    if (xQueueSend(dataModifierQueue, &addEvent, pdMS_TO_TICKS(20)) != pdPASS) {
        ESP_LOGW(TAG, "dataModifierQueue full! Dropping incoming frame.");
        return 1;
    }
    return 0;
}
