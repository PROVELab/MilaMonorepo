#include <stdint.h>

#include "../../programConstants.h"
#include "../vitalsHelper/vitalsHelper.h"
#include "../vitalsGen/vitalsStructs.h"
#include "../vitalsGen/vitalsPacketSendLUT.h"

#include "../contactorControl.h"
#include "vitalsDataHelper.h"

#include "esp_log.h"

const char* TAG = "bounds Check";

static void decrement_crit_count(_Atomic uint8_t* crit_count_ptr);

/**
 * @brief Computes the min/max values from the last N historical data points,
 *        and optionally calculates outlier detection bounds.
 */
void get_outlier_bounds(const int32_t* arr, int32_t* out_lower_bound, int32_t* out_upper_bound) {

    int32_t current_min = INT32_MAX;
    int32_t current_max = INT32_MIN;

    // We look at the N points before the current `dataLocation`
    for (int i = 0; i < pointsPerData; i++) {
        const int32_t val = arr[i];
        if (val < current_min) current_min = val;
        if (val > current_max) current_max = val;
    }

    const int32_t dynamic_range = current_max - current_min;
    // Use a 12.5% margin on the dynamic range to detect outliers, with a small floor.
    const int32_t margin = (dynamic_range / 8) + 16;
    *out_upper_bound = current_max + margin;
    *out_lower_bound = current_min - margin;
}

/**
 * @brief Checks if a value is a significant outlier compared to its normal range.
 */
bool is_outlier(const int32_t* arr, int32_t new_value) {
    int32_t lower_bound, upper_bound;
    get_outlier_bounds(arr, &lower_bound, &upper_bound);
    if (new_value > upper_bound || new_value < lower_bound) {
        return true;
    }
    return false;
}


//check if Critical, and send appropriate warning if needed
bool critical_out_of_bounds(dataPoint* dp, senddataWarningArgs* args, int32_t value){
    critical_dataPoint* critical = dp->criticalStructPtr;
    if (critical == NULL) {
        return false;
    }

    if(value < critical->minCritical || value > critical->maxCritical){
        const uint8_t new_crit_count = atomic_fetch_add(&critical->crit_count, 1) + 1;
        if (critical->crit_count_max > 0 && new_crit_count > critical->crit_count_max) {
            args->dataErrorTrigger.e = confirmedCritical;  //indicate this is a confirmed critical (vitals will turn off car)
            // sendContactorControlCommand(disableContactors);  //TODO comment back when
            // ESP_LOGE(TAG, "confirmed critical sending disable contactors");
            ESP_LOGW(TAG, "not disabling :).");
        }
        senddataWarningFunction(*args);
        return true;
    }
    return false;
}

void processNonCriticalPoint(dataPoint* dp, senddataWarningArgs* args, int32_t value){
    //non-critical, reduce crit counter.
    critical_dataPoint* critical = dp->criticalStructPtr;
    if (critical != NULL) {
        decrement_crit_count(&critical->crit_count);
    }
    
    //check for warning
    if(value < dp->minWarning || value > dp->maxWarning){
        if(!dp->inWarningState){
            dp->inWarningState = true; //update internally to indicate this dataPoint is in warningState
            args->dataErrorTrigger.e = enteredWarningRange;
            senddataWarningFunction(*args);
        }
    } else{
        dp->inWarningState = false;
    }
}

/**
 * @brief Evaluates a value and its future extrapolations against warning/critical bounds
 *        and sends a dataWarningPacket if the value is not OK.
 */
void evaluate_bounds_and_send_warning(dataPoint* dp, CANFrame* frame, int data_idx, int32_t current_value) {
    const int32_t nodeID = (int32_t)vitalsIndexToID((uint32_t)frame->nodeIndex);
    int32_t localFrameID = 0;
    if (!vitalsGlobalFrameIDToLocal(frame, &localFrameID)) {
        return;
    }
    //prepare message
    senddataWarningArgs args = {
        .mask = 0,
        .dataTooHigh = current_value > dp->maxWarning,
        .dataErrorTrigger = {.e = singleCritical},
        .nodeID = nodeID,
        .frameID = localFrameID,
        .dataID = (int32_t)data_idx
    };
    if(mightBeCritical(dp)){
        //check for critical
        int32_t extrap4_val = extrap4Func(frame, data_idx);
        int32_t extrap8_val = extrap8Func(frame, data_idx);
        if(critical_out_of_bounds(dp, &args, current_value)
            || (args.dataErrorTrigger.e = extrap4, critical_out_of_bounds(dp, &args, extrap4_val))
            || (args.dataErrorTrigger.e = extrap8, critical_out_of_bounds(dp, &args, extrap8_val))
        ){
            return;
        }
    }
    processNonCriticalPoint(dp, &args, current_value);
}

bool mightBeCritical(dataPoint* dp){
    return dp->criticalStructPtr != NULL;
}

static void decrement_crit_count(_Atomic uint8_t* crit_count_ptr) {
    uint8_t current_val = atomic_load(crit_count_ptr);
    while (current_val > 0) {
        if (atomic_compare_exchange_weak(crit_count_ptr, &current_val, current_val - 1)) {
            break; // Success
        }
    }
}
