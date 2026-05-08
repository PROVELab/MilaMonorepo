#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "../pecan/pecan.h"
#include "../programConstants.h"
#include "vitalsHelper/vitalsHelper.h"
#include "vitalsHelper/vitalsPacketSendLUT.h"
#include "vitalsHelper/vitalsStaticDec.h"
#include <atomic>

TimerHandle_t missingDataTimers[totalNumFrames];  // one of these timers going off trigers callback function for missing
                                                  // CAN Data Frane
static const char* TAG = "VitalsData";
StaticTimer_t xTimerBuffers[totalNumFrames];      // array for the buffers of these timers
static void vTimerCallback(TimerHandle_t xTimer); // callback for CanFrame Timeouts

typedef enum { DATA_OK, DATA_WARNING, DATA_CRITICAL, DATA_OUTLIER } DataStatus;

static int32_t extrapolateData(CANFrame* frame, int data_idx, int num_points) {
    if (num_points < 2 || num_points > 10) {
        return frame->data[data_idx][(frame->dataLocation - 1 + 10) % 10]; // return last known value if invalid params
    }

    double sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0;

    for (int i = 0; i < num_points; i++) {
        int x = i;
        int data_buf_idx = (frame->dataLocation - (num_points - i) + 10) % 10;
        double y = (double) frame->data[data_idx][data_buf_idx];

        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_x2 += x * x;
    }

    double denominator = (num_points * sum_x2) - (sum_x * sum_x);
    if (denominator == 0) {
        return frame->data[data_idx][(frame->dataLocation - 1 + 10) % 10]; // Cannot extrapolate, return last value
    }

    double slope = ((num_points * sum_xy) - (sum_x * sum_y)) / denominator;
    double intercept = (sum_y - slope * sum_x) / num_points;

    // Extrapolate for the next point (at x = num_points)
    return (int32_t) (slope * num_points + intercept);
}

static DataStatus checkBounds(dataPoint* dp, CANFrame* frame, int data_idx, int32_t new_value, bool is_extrapolated) {
    // Outlier check for non-extrapolated data
    if (!is_extrapolated) {
        int32_t range = dp->max - dp->min;
        if (range > 0) { // Avoid issues if max == min
            int32_t upper_outlier = dp->max + (range * 3);
            int32_t lower_outlier = dp->min - (range * 3);
            if (new_value > upper_outlier || new_value < lower_outlier) {
                ESP_LOGW(TAG, "Outlier detected for node %d, frame %d, data %d. Value: %ld", frame->nodeID,
                         frame->frameID, data_idx, new_value);
                return DATA_OUTLIER;
            }
        }
    }

    // Critical check
    if (dp->minCritical != dp->maxCritical && (new_value <= dp->minCritical || new_value >= dp->maxCritical)) {
        dp->crit_count.fetch_add(1);
        uint8_t current_crit_count = dp->crit_count.load();

        // "if more than 2 critical, shut off"
        if (current_crit_count > 2) {
            ESP_LOGE(TAG, "Critical count exceeded for node %d, frame %d, data %d.", frame->nodeID, frame->frameID,
                     data_idx);
            return DATA_CRITICAL; // Shut off condition
        }

        // Extrapolation check on critical value
        if (extrapolateData(frame, data_idx, 5) <= dp->minCritical ||
            extrapolateData(frame, data_idx, 5) >= dp->maxCritical) {
            ESP_LOGE(TAG, "5-point extrapolation is critical for node %d, frame %d, data %d.", frame->nodeID,
                     frame->frameID, data_idx);
            return DATA_CRITICAL; // Shut off condition
        }
        if (extrapolateData(frame, data_idx, 10) <= dp->minCritical ||
            extrapolateData(frame, data_idx, 10) >= dp->maxCritical) {
            ESP_LOGE(TAG, "10-point extrapolation is critical for node %d, frame %d, data %d.", frame->nodeID,
                     frame->frameID, data_idx);
            return DATA_CRITICAL; // Shut off condition
        }
        return DATA_CRITICAL;
    }

    // Warning check
    if (dp->minWarning != dp->maxWarning && (new_value <= dp->minWarning || new_value >= dp->maxWarning)) {
        dp->crit_count.store(0); // Reset critical counter on non-critical value
        return DATA_WARNING;
    }

    // If we reach here, data is OK
    dp->crit_count.store(0); // Reset critical counter
    return DATA_OK;
}

 // for now just stores the data (printing the past 10 node-frame- data (past 10) on each line)
int16_t monitorData(CANPacket* message) {
    int16_t nodeId = IDTovitalsIndex(message->id);
    if (nodeId == invalidVitalsIndex) {
        ESP_LOGW(TAG, "Received data from invalid nodeId %lu, ignoring", message->id);
        return 1;
    }

    vitalsNode* node = &(nodes[nodeId]); // the node which sent the message

    uint32_t CanFrameNumber = getDataFrameId(message->id); // the Can frame index is stored in extension
    if (CanFrameNumber > node->numFrames) {
        ESP_LOGW(TAG, "Invalid dataFrame %lu for node %d. Ignoring data", CanFrameNumber, nodeId);
        return 1;
    }
    ESP_LOGD(TAG, "Received data for node %d, frame %lu", nodeId, CanFrameNumber);
    CANFrame* frame = &(node->CANFrames[CanFrameNumber]); // the frame this data corresponds to
    // mark this data as collected.

    if (xTimerReset(missingDataTimers[frame->frameID], pdMS_TO_TICKS(100)) == pdFAIL) { // wait up to 100ms to reset timer
        ESP_LOGW(TAG, "Unable to reset data timer for node %d frame %lu", nodeId, CanFrameNumber);
    } else {
        ESP_LOGD(TAG, "Timer reset for node: %d, frame: %ld", nodeId, CanFrameNumber);
    }

    // This is where the new value will be stored
    int next_data_loc = frame->dataLocation;

    // parse each data from frame
    int8_t bitIndex = 0; // which bit of CANFrame we are currently reading from (as we iterate through the data)
    for (int i = 0; i < (*frame).numData; i++) {
        // Unpack one data point at a time. This function handles copying the data,
        // adding the min offset, and incrementing the bit index.
        dataPoint* current_dataPoint = frame->dataInfo + i;
        simpleDataPoint temp_simpleDataPoint = {
            .min = current_dataPoint->min,
            .max = current_dataPoint->max,
            .bits = current_dataPoint->bits
        };

        int32_t new_value;
        pecan_unpack(&new_value, message->data, &temp_simpleDataPoint, &bitIndex);

        DataStatus status = checkBounds(current_dataPoint, frame, i, new_value, false);

        if (status == DATA_CRITICAL || status == DATA_WARNING || status == DATA_OUTLIER) {
            senddataWarningArgs args = {.isCritical = (status == DATA_CRITICAL),
                                        .data_too_high = (new_value > (current_dataPoint->max - current_dataPoint->min) / 2 + current_dataPoint->min),
                                        .extrapolationTrigger = 0, // Not from extrapolation
                                        .nodeID = frame->nodeID,
                                        .frameID = frame->frameID,
                                        .dataID = i};
            senddataWarningFunction(args);
        }

        if (status != DATA_OUTLIER) {
            frame->data[i][next_data_loc] = new_value;
            ESP_LOGD(TAG, "recD: %ld", new_value);
        }
    }
    // increment dataLocation, mark that we have recorded the data:
    frame->dataLocation = (next_data_loc + 1) % pointsPerData;
    frame->consecutiveMisses = 0;
    return 0;
}

void initializeDataTimers() { // initializes timeOuts for Data collection, as soon as this runs, we need data from every
                              // node to be sending their data to prevent them getting flagged, or Bus off if critical
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
    ESP_LOGI(TAG, "Number of data timers initialized: %ld", numInits);
}

static void vTimerCallback(TimerHandle_t xTimer) { // called when data is never recieved. Triggers extrapolation, and
                                                   // extrapolation warning, sent directly to telem
    CANFrame* missingFrame = (CANFrame*) pvTimerGetTimerID(xTimer);
    ESP_LOGW(TAG, "Missing data frame number: %d from node %d.", missingFrame->frameID, missingFrame->nodeID);
    // TODO: Add code or fnct call here to trigger extrapolation
    // Vitals does not yet actually monitor data (I was hoping to get som1 else to do it for me, since its a fairly open
    // and closed function,
    //  but I might end up doing it anyway). Also it would be good to get other people to agree on the algorithm
    //  (whether it be mine, theres, or a mix of both)

    // Send warning for extrapolation
    //TODO: uddate this to send dataWarning with extrapolation trigger flag!
    // sendWarningForDataPoint(missingFrame, 0, missingFrameFlag | nonCriticalWarning);
}
