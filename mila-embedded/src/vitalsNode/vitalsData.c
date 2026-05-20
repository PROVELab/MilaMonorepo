#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "esp_log.h"
#include "../pecan/pecan.h"
#include "../programConstants.h"
#include "vitalsHelper/vitalsHelper.h"
#include "vitalsHelper/vitalsPacketSendLUT.h"
#include "vitalsHelper/vitalsStaticDec.h"
#include <inttypes.h>

TimerHandle_t missingDataTimers[totalNumFrames];  // one of these timers going off trigers callback function for missing
                                                  // CAN Data Frane
static const char* TAG = "VitalsData";
StaticTimer_t xTimerBuffers[totalNumFrames];      // array for the buffers of these timers
static void vTimerCallback(TimerHandle_t xTimer); // callback for CanFrame Timeouts

typedef enum {
    DATA_OK,
    DATA_OUTLIER,
    DATA_WARNING,
    DATA_CRITICAL,
    DATA_CRITICAL_EXTRAP_5,
    DATA_CRITICAL_EXTRAP_10,
    DATA_CRITICAL_PERSISTENT
} DataStatus;

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

/**
 * @brief Atomically decrements a uint8_t counter, saturating at 0.
 *
 * This uses a compare-and-swap (CAS) loop to safely decrement the counter
 * in a multi-threaded environment, preventing underflow.
 */
static void decrement_crit_count(_Atomic uint8_t* crit_count_ptr) {
    uint8_t current_val = atomic_load(crit_count_ptr);
    while (current_val > 0) {
        if (atomic_compare_exchange_weak(crit_count_ptr, &current_val, current_val - 1)) {
            break; // Success
        }
    }
}

static DataStatus checkBounds(dataPoint* dp, CANFrame* frame, int data_idx, int32_t new_value, bool is_extrapolated) {
    // Outlier check for non-extrapolated data
    if (!is_extrapolated) {
        int32_t range = dp->max - dp->min;
        if (range > 0) { // Avoid issues if max == min
            int32_t upper_outlier = dp->max + (range * 3);
            int32_t lower_outlier = dp->min - (range * 3);
            if (new_value > upper_outlier || new_value < lower_outlier) {
                ESP_LOGW(TAG, "Outlier detected for node %d, frame %d, data %d. Value: %" PRIi32, frame->nodeID,
                         frame->frameID, data_idx, new_value);
                return DATA_OUTLIER;
            }
        }
    }

    // If no critical range is defined, we can't have a critical error.
    if (dp->minCritical == dp->maxCritical) {
        if (dp->minWarning != dp->maxWarning && (new_value <= dp->minWarning || new_value >= dp->maxWarning)) {
            decrement_crit_count(&dp->crit_count);
            return DATA_WARNING;
        }
        decrement_crit_count(&dp->crit_count);
        return DATA_OK;
    }

    // --- Critical Range is defined ---

    // Perform extrapolations once to avoid redundant calculations.
    int32_t extrap5_val = extrapolateData(frame, data_idx, 5);
    int32_t extrap10_val = extrapolateData(frame, data_idx, 10);

    // Check if any critical condition is met.
    if ((new_value <= dp->minCritical || new_value >= dp->maxCritical) ||
        (extrap5_val <= dp->minCritical || extrap5_val >= dp->maxCritical) ||
        (extrap10_val <= dp->minCritical || extrap10_val >= dp->maxCritical)) {
        uint8_t new_crit_count = atomic_fetch_add(&dp->crit_count, 1) + 1;
        if (dp->crit_count_max > 0 && new_crit_count > dp->crit_count_max) {
            return DATA_CRITICAL_PERSISTENT;
        }
        // Return the most severe error type.
        if (extrap10_val <= dp->minCritical || extrap10_val >= dp->maxCritical) {
            return DATA_CRITICAL_EXTRAP_10;
        }
        if (extrap5_val <= dp->minCritical || extrap5_val >= dp->maxCritical) {
            return DATA_CRITICAL_EXTRAP_5;
        }
        return DATA_CRITICAL; // Must be the new_value that was critical.
    }

    // If not critical, check for warning.
    if (dp->minWarning != dp->maxWarning && (new_value <= dp->minWarning || new_value >= dp->maxWarning)) {
        decrement_crit_count(&dp->crit_count);
        return DATA_WARNING;
    }

    // If we reach here, data is OK.
    decrement_crit_count(&dp->crit_count);
    return DATA_OK;
}

 // for now just stores the data (printing the past 10 node-frame- data (past 10) on each line)
int16_t monitorData(CANPacket* message) {
    int16_t nodeId = IDTovitalsIndex(message->id);
    if (nodeId == invalidVitalsIndex) { // Note: message->id is int32_t
        ESP_LOGW(TAG, "Received data from invalid nodeId %" PRIi32 ", ignoring", message->id);
        return 1;
    }

    vitalsNode* node = &(nodes[nodeId]); // the node which sent the message

    uint32_t CanFrameNumber = getDataFrameId(message->id); // the Can frame index is stored in extension
    if (CanFrameNumber > node->numFrames) {
        ESP_LOGW(TAG, "Invalid dataFrame %" PRIu32 " for node %d. Ignoring data", CanFrameNumber, nodeId);
        return 1;
    }
    ESP_LOGD(TAG, "Received data for node %d, frame %" PRIu32, nodeId, CanFrameNumber);
    CANFrame* frame = &(node->CANFrames[CanFrameNumber]); // the frame this data corresponds to
    // mark this data as collected.

    if (xTimerReset(missingDataTimers[frame->frameID], pdMS_TO_TICKS(100)) == pdFAIL) { // wait up to 100ms to reset timer
        ESP_LOGW(TAG, "Unable to reset data timer for node %d frame %" PRIu32, nodeId, CanFrameNumber);
    } else {
        ESP_LOGD(TAG, "Timer reset for node: %d, frame: %" PRIu32, nodeId, CanFrameNumber);
    }

    // This is where the new value will be stored
    int next_data_loc = frame->dataLocation;

    // State for aggregating warnings for this frame
    bool frame_has_issue = false;
    DataStatus most_severe_status = DATA_OK;
    int32_t problematic_data_id = -1;
    int32_t problematic_value = 0;

    // parse each data from frame
    int8_t bitIndex = 0; // which bit of CANFrame we are currently reading from (as we iterate through the data)
    for (int i = 0; i < (*frame).numData; i++) {
        // Unpack one data point at a time. This function handles copying the data,
        // adding the min offset, and incrementing the bit index.
        dataPoint* current_dataPoint = frame->dataInfo + i;
        int32_t new_value;
        // This cast is now safe because dataPoint and simpleDataPoint are standard-layout
        // and share a common initial sequence.
        pecan_unpack(&new_value, message->data, (simpleDataPoint*)current_dataPoint, &bitIndex);

        DataStatus status = checkBounds(current_dataPoint, frame, i, new_value, false);

        if (status != DATA_OK) {
            frame_has_issue = true;
            if (status > most_severe_status) {
                most_severe_status = status;
                problematic_data_id = i;
                problematic_value = new_value;
            }
        }

        if (status != DATA_OUTLIER) {
            frame->data[i][next_data_loc] = new_value;
            ESP_LOGD(TAG, "recD: %" PRIi32, new_value);
        }
    }

    // After checking all data points, send a single warning if any issue was found.
    if (frame_has_issue) {
        errorTrigger trigger_enum = warning_nonCritical;
        if (most_severe_status == DATA_CRITICAL) {
            trigger_enum = singleCritical;
        }
        if (most_severe_status == DATA_CRITICAL_EXTRAP_5) {
            trigger_enum = extrap5;
        } else if (most_severe_status == DATA_CRITICAL_EXTRAP_10) {
            trigger_enum = extrap10;
        } else if (most_severe_status == DATA_CRITICAL_PERSISTENT) {
            trigger_enum = confirmedCritical;
        }

        dataPoint* problematic_dp = (problematic_data_id != -1) ? &frame->dataInfo[problematic_data_id] : NULL;
        bool data_too_high = (problematic_dp != NULL) ? (problematic_value > (problematic_dp->max - problematic_dp->min) / 2 + problematic_dp->min) : false;

        senddataWarningArgs args = {
            .mask = 0, // Will be set by the auto-generated function
            .data_too_high = data_too_high,
            .extrapolationDueToTimeout = false,
            .errorTrigger = {.e = trigger_enum},
            .nodeID = (int32_t)frame->nodeID,
            .frameID = (int32_t)frame->frameID,
            .dataID = (int32_t)problematic_data_id
        };
        senddataWarningFunction(args);
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
    ESP_LOGI(TAG, "Number of data timers initialized: %" PRIi32, numInits);
}

static void vTimerCallback(TimerHandle_t xTimer) { // called when data is never recieved. Triggers extrapolation, and
                                                   // extrapolation warning, sent directly to telem
    CANFrame* missingFrame = (CANFrame*) pvTimerGetTimerID(xTimer);
    ESP_LOGW(TAG, "Missing data frame number: %d from node %d.", missingFrame->frameID, missingFrame->nodeID);

    // Send a warning for the missing frame.
    // This reports on data point 0 of the frame as a representative.
    senddataWarningArgs args = {
        .mask = 0,
        .data_too_high = false,
        .extrapolationDueToTimeout = true,
        .errorTrigger = {.e = warning_nonCritical},
        .nodeID = (int32_t)missingFrame->nodeID,
        .frameID = (int32_t)missingFrame->frameID,
        .dataID = 0
    };
    senddataWarningFunction(args);
}
