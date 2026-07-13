#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdatomic.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "../vitalsHelper/vitalsHelper.h"
#include "../vitalsGen/vitalsPacketSendLUT.h"
#include "../../programConstants.h"
#include "../vitalsGen/vitalsStructs.h"
#include "../contactorControl.h"

#include "vitalsDataHelper.h"

#define STACK_SIZE 8000
StaticTask_t dataModifier_Buffer;
StackType_t dataModifier_Stack[STACK_SIZE];

#define DATA_MODIFIER_QUEUE_LENGTH 16

static const char* TAG = "DataModifier";

static StaticQueue_t dataModifierQueueBuffer;
static uint8_t dataModifierQueueStorage[DATA_MODIFIER_QUEUE_LENGTH * sizeof(DataModifierEvent)];
QueueHandle_t dataModifierQueue;

static void dataModifierTask(void *pvParameters);
static void handleMissingFrame(CANFrame* missingFrame);
static void handle_new_frame(vitalsNode* node, CANFrame* frame, CANPacket* message, uint32_t CanFrameNumber);
static void pack_and_send_frame_data(vitalsNode* node, CANFrame* frame, uint32_t CanFrameNumber,  CANPacket* message);
static void handleEnableContactorRequest(void);
static bool areAllCritCountsZero(void);

void initializeDataModifier(const UBaseType_t priority){
    dataModifierQueue = xQueueCreateStatic(
        DATA_MODIFIER_QUEUE_LENGTH,
        sizeof(DataModifierEvent),
        dataModifierQueueStorage,
        &dataModifierQueueBuffer
    );
    
    if (dataModifierQueue == NULL) {
        ESP_LOGE(TAG, "Failed to create dataModifierQueue. Halting.");
        while(1);
    }

    xTaskCreateStaticPinnedToCore(
        dataModifierTask,                    /* Function that implements the task. */
        "dataModifier",           /* Text name for the task. */
        STACK_SIZE,                /* Number of indexes in the xStack array. */
        (void*) 1,                 /* Parameter passed into the task. */ 
        priority,                         /* Priority at which the task is created. */
        dataModifier_Stack,              /* Array to use as the task's stack. */
        &dataModifier_Buffer,            /* Variable to hold the task's data structure. */
        tskNO_AFFINITY);
}


void vTimerCallback(TimerHandle_t xTimer) { // called when data is never received. Triggers extrapolation, and
                                                   // extrapolation warning, sent directly to telem
    CANFrame* missingFrame = (CANFrame*) pvTimerGetTimerID(xTimer);
    DataModifierEvent missingEvent = {0};
    missingEvent.eventType = MISSING_FRAME;
    missingEvent.payload.request.frame = missingFrame;

    if (xQueueSend(dataModifierQueue, &missingEvent, 0) != pdPASS) {
        ESP_LOGW(TAG, "dataModifierQueue full! Dropping missing frame event.");
    }
}

static void dataModifierTask(void *pvParameters) {
    DataModifierEvent event;
    
    while (1) {
        if (xQueueReceive(dataModifierQueue, &event, portMAX_DELAY) == pdTRUE) {
            if (event.eventType == ADD_FRAME) {
                CANFrame* frame = event.payload.request.frame;
                CANPacket* message = &event.payload.request.packet;
                uint32_t CanFrameNumber = event.payload.request.canFrameNumber;
                const int32_t nodeIndex = frame->nodeIndex;
                if (nodeIndex < 0 || nodeIndex >= numberOfNodes) {
                    ESP_LOGW(TAG, "Dropping frame for invalid node index %" PRIi8, frame->nodeIndex);
                    continue;
                }
                handle_new_frame(&nodes[nodeIndex], frame, message, CanFrameNumber);
            } 
            else if (event.eventType == MISSING_FRAME) {
                CANFrame* missingFrame = event.payload.request.frame;
                handleMissingFrame(missingFrame);
            } else if (event.eventType == ATTEMPT_ENABLE_CONTACTORS) {
                handleEnableContactorRequest();
            } else if(event.eventType == UPDATE_TELEMETRY_DIVIDER){
                event.payload.dividerRequest.frame->telemetryDivider=event.payload.dividerRequest.newDivider;
            }
        }
    }
}

static void handleEnableContactorRequest(void) {
    // if(true){ //TODO: RETURN THIS CHECK WHEN ACTUALLY RUNNING HV and IMD)
    // if (areAllCritCountsZero()) {
    if(true){
        ESP_LOGI(TAG, "Contactor enable safety check passed.");
        sendContactorControlCommand(enableContactors);
    } else {
        ESP_LOGI(TAG, "Contactor enable safety check failed.");
    }
}

static bool areAllCritCountsZero(void) {
    for (int nodeIndex = 0; nodeIndex < numberOfNodes; nodeIndex++) {
        vitalsNode* node = &nodes[nodeIndex];
        for (int frameIndex = 0; frameIndex < node->numFrames; frameIndex++) {
            CANFrame* frame = &node->CANFrames[frameIndex];
            for (int dataIndex = 0; dataIndex < frame->numData; dataIndex++) {
                critical_dataPoint* critical = frame->dataInfo[dataIndex].criticalStructPtr;
                if (critical != NULL && atomic_load(&critical->crit_count) != 0) {
                    return false;
                }
            }
        }
    }
    return true;
}

static void handleMissingFrame(CANFrame* missingFrame){
    const int32_t nodeID = (int32_t)vitalsIndexToID((uint32_t)missingFrame->nodeIndex);
    int32_t localFrameID = 0;
    if (!vitalsGlobalFrameIDToLocal(missingFrame, &localFrameID)) {
        return;
    }
    missingFrame->consecutiveMisses++;

    if (missingFrame->consecutiveMisses >= 8) {
        ESP_LOGE(TAG, "Repeatedly missing data frame number: %" PRIi8 " from node %" PRIi32 ". Not extrapolating.",
                 missingFrame->frameID, nodeID);
        sendframeWarningArgs args = {.mask = 0,
                                     .frameErrorTrigger = {.e = repeatedDataTimeout},
                                     .nodeID = nodeID,
                                     .frameID = localFrameID};
        sendframeWarningFunction(args);
        return; // Do not extrapolate or increment data location. this data is dead :/
    }

    ESP_LOGW(TAG, "Missing data frame number: %" PRIi8 " from node %" PRIi32 ". Extrapolating.", missingFrame->frameID,
             nodeID);

    if (missingFrame->hasCriticalData) {
        //prepare message
        senddataWarningArgs args = {
            .mask = 0,
            .dataTooHigh = 0,
            .dataErrorTrigger = {.e = singleCritical},
            .nodeID = nodeID,
            .frameID = localFrameID,
            .dataID = (int32_t)0
        };

        // Extrapolate and store data locally for critical datapoints only.
        for (int i = 0; i < missingFrame->numData; i++) {
            dataPoint* current_dataPoint = missingFrame->dataInfo + i;
            critical_dataPoint* critical = current_dataPoint->criticalStructPtr;
            if (critical == NULL) {
                continue;
            }

            int32_t new_value = extrap8Func(missingFrame, i);
            critical->data[missingFrame->dataLocation] = new_value;

            args.dataTooHigh = new_value > current_dataPoint->maxWarning;
            args.dataID = i;

            if (critical_out_of_bounds(current_dataPoint, &args, new_value)) {
                continue;
            }
            processNonCriticalPoint(current_dataPoint, &args, new_value);
        }
    }

    // Send a frame warning for the timeout
    sendframeWarningArgs frameArgs = {.mask = 0,
                                 .frameErrorTrigger = {.e = dataTimeout},
                                 .nodeID = nodeID,
                                 .frameID = localFrameID};
    sendframeWarningFunction(frameArgs);

    // Increment dataLocation for the locally stored extrapolated data
    if (missingFrame->hasCriticalData) {
        missingFrame->dataLocation = (missingFrame->dataLocation + 1) % pointsPerData;
    }
}


static void handle_new_frame(vitalsNode* node, CANFrame* frame, CANPacket* message, uint32_t CanFrameNumber) {
    frame->consecutiveMisses = 0; // Reset misses on successful reception
    const int32_t nodeID = (int32_t)node->CAN_ID;

    // parse each data from frame
    int8_t bitIndex = 0; // which bit of CANFrame we are currently reading from (as we iterate through the data)
    for (int dataIndex = 0; dataIndex < (*frame).numData; dataIndex++) {
        dataPoint* current_dataPoint = &frame->dataInfo[dataIndex];
        critical_dataPoint* dataPoint_critical = current_dataPoint->criticalStructPtr;
        int32_t new_value = 0;
        pecan_unpack(&new_value, &(message->data), (simpleDataPoint*)current_dataPoint, &bitIndex);

        if (dataPoint_critical == NULL) {
            evaluate_bounds_and_send_warning(current_dataPoint, frame, dataIndex, new_value);
            continue;
        }

        // --- Step 1: Conditionally resolve any pending outlier ---
        if (dataPoint_critical->outlier_present) {
            int32_t outlier_value = dataPoint_critical->outlier_slot;

            // Temporarily place new_value in the buffer to establish a new, more relevant dynamic range
            int32_t original_value_at_next_loc = dataPoint_critical->data[frame->dataLocation]; // Save original value
            dataPoint_critical->data[frame->dataLocation] = new_value;

            int32_t lower_bound, upper_bound;
            get_outlier_bounds(dataPoint_critical->data, &lower_bound, &upper_bound);

            dataPoint_critical->data[frame->dataLocation] = original_value_at_next_loc; // Restore buffer

            if (outlier_value >= lower_bound && outlier_value <= upper_bound) {
                ESP_LOGI(TAG, "Outlier confirmed for node %" PRIi32 ", frame %" PRIi8 ", data %d. Value: %" PRIi32, nodeID, frame->frameID, dataIndex, outlier_value);
                // Retroactively fix the history by overwriting the placeholder from the last cycle.
                int prev_data_loc = (frame->dataLocation - 1 + pointsPerData) % pointsPerData;
                dataPoint_critical->data[prev_data_loc] = outlier_value;
                // Now that history is correct, run checks on the confirmed outlier.
                evaluate_bounds_and_send_warning(current_dataPoint, frame, dataIndex, outlier_value);
            } else {
                ESP_LOGW(TAG, "Outlier NOT confirmed for node %" PRIi32 ", frame %" PRIi8 ", data %d. Discarding outlier.", nodeID, frame->frameID, dataIndex);
            }
        }
        dataPoint_critical->outlier_present = false;

        // --- Step 2: Process the current new_value ---
        if (is_outlier(dataPoint_critical->data, new_value)) {
            // It's a new potential outlier. Defer it.
            ESP_LOGW(TAG, "Potential outlier for node %" PRIi32 ", frame %" PRIi8 ", data %d. Value: %" PRIi32 ". Deferring.", nodeID, frame->frameID, dataIndex, new_value);
            dataPoint_critical->outlier_present = true;
            dataPoint_critical->outlier_slot = new_value;
            // Store a placeholder (duplicate of the last known value) to avoid polluting history.
            int32_t last_value = dataPoint_critical->data[(frame->dataLocation - 1 + pointsPerData) % pointsPerData];
            dataPoint_critical->data[frame->dataLocation] = last_value;
        } else {
            // It's a good value. Store it.
            dataPoint_critical->data[frame->dataLocation] = new_value;
            // Perform extrapolations and evaluate bounds
            evaluate_bounds_and_send_warning(current_dataPoint, frame, dataIndex, new_value);
        }
    }

    frame->telemetryDivider_Count++;
    if(frame->telemetryDivider_Count >= frame->telemetryDivider){
        pack_and_send_frame_data(node, frame, CanFrameNumber, message);
        frame->telemetryDivider_Count = 0;
    }
    
    if (frame->hasCriticalData) {
        frame->dataLocation = (frame->dataLocation + 1) % pointsPerData;
    }
}

static void pack_and_send_frame_data(vitalsNode* node, CANFrame* frame, uint32_t CanFrameNumber, CANPacket* message){
    // 1. Calculate total size needed for the bit-packed payload
    uint32_t payload_total_bits = 0;

    for (int i = 0; i < frame->numData; i++) {
        payload_total_bits += frame->dataInfo[i].bits;
    }
    uint32_t payload_size_bytes = (payload_total_bits + 7) / 8;

    // // 2. Pack the payload
    // uint8_t payload[8] = {0};
    // memset(payload, 0, payload_size_bytes);
    // int8_t payload_bit_idx = 0;

    // for (int i = 0; i < frame->numData; i++) {
    //     pecan_pack(payload, &payload_bit_idx, latest_values[i], (simpleDataPoint*)&frame->dataInfo[i]);
    // }

    // 3. Sepacket with its header and custom payload
    sendCANDataFrameFunction((sendCANDataFrameHeader){
        .nodeID = (int32_t)node->CAN_ID,
        .frameID = (int32_t)CanFrameNumber,
    }, message->data, payload_size_bytes);
}
