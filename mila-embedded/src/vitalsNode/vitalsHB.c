#include "driver/gpio.h"
#include "driver/twai.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "../pecan/pecan.h"       //helper code for CAN stuff
#include "../programConstants.h"
#include "vitalsHelper/vitalsHelper.h"
#include "vitalsGen/vitalsPacketSendLUT.h"
#include "vitalsGen/vitalsStructs.h"

static const char* TAG = "VitalsHB";
static void printAllData(); // not for final use. for testing only

#define STACK_SIZE 8000
static void checkHB(void* pvParameters);
StaticTask_t checkHB_Buffer;
StackType_t checkHB_Stack[STACK_SIZE];

int64_t HBSendTime = 0;
void sendHB(void* pvParameters) {
    // creates the checkHB task
    TaskHandle_t processHBResp = xTaskCreateStaticPinnedToCore( // checksHB responses
        checkHB,                                                /* Function that implements the task. */
        "checkHeartBeatResponses",                              /* Text name for the task. */
        STACK_SIZE,                                             /* Number of indexes in the xStack array. */
        (void*) 1, /* Parameter passed into the task. */ // should only use constants here. Global variables may be ok?
                                                         // cant be a stack variable.
        tskIDLE_PRIORITY,                                /* Priority at which the task is created. */
        checkHB_Stack,                                   /* Array to use as the task's stack. */
        &checkHB_Buffer,                                 /* Variable to hold the task's data structure. */
        tskNO_AFFINITY);                                 // assigns printHello to core 0

    for (;;) {
        // Send HB
        CANPacket message = {0};
        setRTR(&message);
        message.id = combinedID(HBPing, vitalsID); // HBPing, vitalsID
        sendPacket(&message);

        HBSendTime = esp_timer_get_time();
        ESP_LOGD(TAG, "Sent HB");
        vTaskResume(processHBResp); // run task to process HB responses
        printAllData();             // for debugging, just printing data periodically to view it.
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

int16_t recieveHeartbeat(CANPacket* message) { // mark the HB for given node as recieved, recording time to respond
    ESP_LOGD(TAG, "Received Pong from: %" PRIi32, (message->id) & 0x7F);
    int16_t nodeIndex = IDTovitalsIndex(message->id);
    if (nodeIndex == invalidVitalsIndex) {
        ESP_LOGW(TAG, "Received HB from invalid nodeId %" PRIi32 ", ignoring", message->id);
        return 0; // invalid id
    }
    // responseTime is in microseconds, scale to milliseconds for a more useful range.
    int64_t responseTime_us = esp_timer_get_time() - HBSendTime;
    int64_t responseTime_ms = responseTime_us / 1000;
    uint16_t responseTime16 = responseTime_ms < 0      ? 0
                              : responseTime_ms > 1023 ? 1023 // Clamp to the 10-bit field max value
                                                       : (uint16_t)responseTime_ms;
    VitalsFlagSet(nodeIndex, HBFlag);
    HBTimeSet(nodeIndex, responseTime16);
    return 0;
}

// Corrected helper to find the 3 slowest nodes.
static inline void updateSlowestIndex(int32_t* slowestNodesArray, int16_t* worstTimesArray, uint8_t nodeIndex, int16_t time) {
    for (int i = 0; i < slowestNodeCount; i++) {
        if (time > worstTimesArray[i]) {
            // Shift elements down
            for (int j = slowestNodeCount - 1; j > i; j--) {
                worstTimesArray[j] = worstTimesArray[j - 1];
                slowestNodesArray[j] = slowestNodesArray[j - 1];
            }
            // Insert new time and index
            worstTimesArray[i] = time;
            slowestNodesArray[i] = nodeIndex;
            return;
        }
    }
}

// two different frames for HB are sent. A timing frame and status frame(s) check diagram (should be in a memo at some
// point for details, of format, or look at the code
static void checkHB(void* pvParameters) {

    for (;;) {
        vTaskDelay(250 / portTICK_PERIOD_MS); // give nodes 250ms to respond
        ESP_LOGD(TAG, "Checking HB responses");

        // Trackers
        int32_t hb_mask = 0;
        int32_t slowestNodeIndices[3] = {0};
        int16_t worstTimes[3] = {0, 0, 0};

        // Process responses
        for (int i = 0; i < numberOfNodes; i++) {
            if (VitalsFlagsGet(i) & HBFlag) {
                hb_mask |= (1 << i);
                VitalsFlagClear(i, HBFlag); // reset HB bit
                int16_t responseTime = HBTimeGet(i);
                updateSlowestIndex(slowestNodeIndices, worstTimes, i, responseTime);
            }
        }

        // Send HBStatus packet
        sendHBStatusArgs status_args = {0, hb_mask}; // {mask, HBMask}
        sendHBStatusFunction(status_args);

        // Send HBTiming packet
        sendHBTimingArgs timing_args = {
            0, // mask
            (worstTimes[0] > 0) ? (int32_t)vitalsIndexToID((uint32_t)slowestNodeIndices[0]) : 0,
            worstTimes[0],
            (worstTimes[1] > 0) ? (int32_t)vitalsIndexToID((uint32_t)slowestNodeIndices[1]) : 0,
            worstTimes[1],
            (worstTimes[2] > 0) ? (int32_t)vitalsIndexToID((uint32_t)slowestNodeIndices[2]) : 0,
            worstTimes[2],
        };
        sendHBTimingFunction(timing_args);

        vTaskSuspend(NULL);
    }
}

static void printAllData() { // not for final use. for testing only
    for (int i = 0; i < numberOfNodes; i++) { // each node
        ESP_LOGD(TAG, "Node %d (ID: %" PRIu32 "), numFrames: %d", i, vitalsIndexToID(i), (nodes[i]).numFrames);
        for (int8_t j = 0; j < nodes[i].numFrames; j++) { // each frame
            ESP_LOGD(TAG, "  Frame %d (ID: %d), numData: %d", j, ((nodes[i]).CANFrames[j]).frameID,
                     ((nodes[i]).CANFrames[j]).numData);
            if ((nodes[i]).CANFrames == NULL) {
                ESP_LOGE(TAG, "  Error: frames pointer is NULL, terminating print.");
                return;
            }
            for (int8_t k = 0; k < (((nodes[i]).CANFrames)[j]).numData; k++) { // each data
                char data_buf[128];
                int offset = snprintf(data_buf, sizeof(data_buf), "    Data %d: ", k);
                for (int l = 0; l < pointsPerData; l++) {
                    offset += snprintf(data_buf + offset, sizeof(data_buf) - offset, "%ld ", nodes[i].CANFrames[j].data[k][l]);
                }
                ESP_LOGD(TAG, "%s", data_buf);
            }
        }
    }
}
