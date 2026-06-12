#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"

#include "../vitalsGen/vitalsPacketSendLUT.h"
#include "../../programConstants.h"
#include "../vitalsGen/vitalsStructs.h"

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
static void pack_and_send_frame_data(vitalsNode* node, CANFrame* frame, uint32_t CanFrameNumber);

void initializeDataModifier(){
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
        3,                         /* Priority at which the task is created. */
        dataModifier_Stack,              /* Array to use as the task's stack. */
        &dataModifier_Buffer,            /* Variable to hold the task's data structure. */
        tskNO_AFFINITY);
}


void vTimerCallback(TimerHandle_t xTimer) { // called when data is never recieved. Triggers extrapolation, and
                                                   // extrapolation warning, sent directly to telem
    CANFrame* missingFrame = (CANFrame*) pvTimerGetTimerID(xTimer);
    // --- PUSH TO QUEUE ---
    DataModifierEvent missingEvent = {
        .eventType = DATA_MODIFIER_MISSING_FRAME,
        .frame = missingFrame
        // canFrameNumber and packet are left uninitialized as they aren't needed here
    };

    // Do not block (0 ticks) in a timer callback
    if (xQueueSend(dataModifierQueue, &missingEvent, 0) != pdPASS) {
        ESP_LOGW(TAG, "dataModifierQueue full! Dropping missing frame event.");
    }
}

// You will need to spin this up via xTaskCreate or xTaskCreateStatic
static void dataModifierTask(void *pvParameters) {
    DataModifierEvent event;
    
    while (1) {
        // Wait indefinitely for an event to enter the queue
        if (xQueueReceive(dataModifierQueue, &event, portMAX_DELAY) == pdTRUE) {
            
            if (event.eventType == DATA_MODIFIER_ADD_FRAME) {
                // --- THE REST OF monitorData() GOES HERE ---
                CANFrame* frame = event.frame;
                CANPacket* message = &event.packet;
                uint32_t CanFrameNumber = event.canFrameNumber;
                handle_new_frame(&nodes[frame->nodeID], frame, message, CanFrameNumber);
                
            } 
            else if (event.eventType == DATA_MODIFIER_MISSING_FRAME) {
                // --- THE REST OF vTimerCallback() GOES HERE ---
                CANFrame* missingFrame = event.frame;
                handleMissingFrame(missingFrame);
            }
        }
    }
}

static void handleMissingFrame(CANFrame* missingFrame){
    missingFrame->consecutiveMisses++;

    if (missingFrame->consecutiveMisses >= 8) {
        ESP_LOGE(TAG, "Repeatedly missing data frame number: %d from node %d. Not extrapolating.",
                 missingFrame->frameID, missingFrame->nodeID);
        sendframeWarningArgs args = {.mask = 0,
                                     .frameErrorTrigger = {.e = repeatedDataTimeout},
                                     .nodeID = (int32_t)missingFrame->nodeID,
                                     .frameID = (int32_t)missingFrame->frameID};
        sendframeWarningFunction(args);
        return; // Do not extrapolate or increment data location. this data is dead :/
    }

    ESP_LOGW(TAG, "Missing data frame number: %d from node %d. Extrapolating.", missingFrame->frameID,
             missingFrame->nodeID);

    //prepare message
    senddataWarningArgs args = {
        .mask = 0,
        .dataTooHigh = 0,
        .dataErrorTrigger = {.e = singleCritical},
        .nodeID = (int32_t)missingFrame->nodeID,
        .frameID = (int32_t)missingFrame->frameID,
        .dataID = (int32_t)0
    };

    // Extrapolate and store data locally
    for (int i = 0; i < missingFrame->numData; i++) {
        dataPoint* current_dataPoint = missingFrame->dataInfo + i;
        
        // extrapolate the new point
        int32_t new_value = extrap8Func(missingFrame, i);
        missingFrame->data[i][missingFrame->dataLocation] = new_value;

        if (new_value > current_dataPoint->maxWarning) {
            args.dataTooHigh = 1;
        }
        args.dataID = i;

        //check if critical
        if(mightBeCritical(current_dataPoint)){
            if(critical_out_of_bounds(current_dataPoint, &args, new_value)){
                continue;
            }
        }
        processNonCriticalPoint(current_dataPoint, &args, new_value);
    }

    // Send a frame warning for the timeout
    sendframeWarningArgs frameArgs = {.mask = 0,
                                 .frameErrorTrigger = {.e = dataTimeout},
                                 .nodeID = (int32_t)missingFrame->nodeID,
                                 .frameID = (int32_t)missingFrame->frameID};
    sendframeWarningFunction(frameArgs);

    // Increment dataLocation for the locally stored extrapolated data
    missingFrame->dataLocation = (missingFrame->dataLocation + 1) % pointsPerData;
}

static void handle_new_frame(vitalsNode* node, CANFrame* frame, CANPacket* message, uint32_t CanFrameNumber) {
    frame->consecutiveMisses = 0; // Reset misses on successful reception
    // parse each data from frame
    int8_t bitIndex = 0; // which bit of CANFrame we are currently reading from (as we iterate through the data)
    for (int dataIndex = 0; dataIndex < (*frame).numData; dataIndex++) {
        dataPoint* current_dataPoint = &frame->dataInfo[dataIndex];
        int32_t new_value;
        pecan_unpack(&new_value, message->data, (simpleDataPoint*)current_dataPoint, &bitIndex);

        // --- Step 1: Conditionally resolve any pending outlier ---
        if (current_dataPoint->outlier_present) {
            int32_t outlier_value = current_dataPoint->outlier_slot;

            // Temporarily place new_value in the buffer to establish a new, more relevant dynamic range
            int32_t original_value_at_next_loc = frame->data[dataIndex][frame->dataLocation]; // Save original value
            frame->data[dataIndex][frame->dataLocation] = new_value;

            int32_t lower_bound, upper_bound;
            get_outlier_bounds(frame->data[dataIndex], &lower_bound, &upper_bound);

            frame->data[dataIndex][frame->dataLocation] = original_value_at_next_loc; // Restore buffer

            if (outlier_value >= lower_bound && outlier_value <= upper_bound) {
                ESP_LOGI(TAG, "Outlier confirmed for node %d, frame %d, data %d. Value: %" PRIi32, frame->nodeID, frame->frameID, dataIndex, outlier_value);
                // Retroactively fix the history by overwriting the placeholder from the last cycle.
                int prev_data_loc = (frame->dataLocation - 1 + pointsPerData) % pointsPerData;
                frame->data[dataIndex][prev_data_loc] = outlier_value;
                // Now that history is correct, run checks on the confirmed outlier.
                evaluate_bounds_and_send_warning(current_dataPoint, frame, dataIndex, outlier_value);
            } else {
                ESP_LOGW(TAG, "Outlier NOT confirmed for node %d, frame %d, data %d. Discarding outlier.", frame->nodeID, frame->frameID, dataIndex);
            }
        }
        current_dataPoint->outlier_present = false;

        // --- Step 2: Process the current new_value ---
        if (is_outlier(frame->data[dataIndex], new_value)) {
            // It's a new potential outlier. Defer it.
            ESP_LOGW(TAG, "Potential outlier for node %d, frame %d, data %d. Value: %" PRIi32 ". Deferring.", frame->nodeID, frame->frameID, dataIndex, new_value);
            current_dataPoint->outlier_present = true;
            current_dataPoint->outlier_slot = new_value;
            // Store a placeholder (duplicate of the last known value) to avoid polluting history.
            int32_t last_value = frame->data[dataIndex][(frame->dataLocation - 1 + pointsPerData) % pointsPerData];
            frame->data[dataIndex][frame->dataLocation] = last_value;
        } else {
            // It's a good value. Store it.
            frame->data[dataIndex][frame->dataLocation] = new_value;
            // Perform extrapolations and evaluate bounds
            evaluate_bounds_and_send_warning(current_dataPoint, frame, dataIndex, new_value);
        }
    }
    pack_and_send_frame_data(node, frame, CanFrameNumber);
    frame->dataLocation = (frame->dataLocation + 1) % pointsPerData;
}

static void pack_and_send_frame_data(vitalsNode* node, CANFrame* frame, uint32_t CanFrameNumber){
    // 1. Calculate total size needed for the bit-packed payload
    uint32_t payload_total_bits = 0;
    uint8_t frame_idx_bits;
    if (node->numFrames <= 1) {
        frame_idx_bits = 1;
    } else {
        // Calculate bits needed for frame index, equivalent to ceil(log2(numFrames))
        frame_idx_bits = 32 - __builtin_clz(node->numFrames - 1);
    }
    payload_total_bits += frame_idx_bits;

    for (int i = 0; i < frame->numData; i++) {
        payload_total_bits += frame->dataInfo[i].bits;
    }
    uint32_t payload_size_bytes = (payload_total_bits + 7) / 8;

    // 2. Pack the payload
    uint8_t payload[payload_size_bytes];
    memset(payload, 0, payload_size_bytes);
    int8_t payload_bit_idx = 0;

    simpleDataPoint frame_idx_sdp = {.min = 0, .max = node->numFrames - 1, .bits = frame_idx_bits};
    pecan_pack(payload, &payload_bit_idx, CanFrameNumber, &frame_idx_sdp);

    for (int i = 0; i < frame->numData; i++) {
        pecan_pack(payload, &payload_bit_idx, frame->data[i][frame->dataLocation], (simpleDataPoint*)&frame->dataInfo[i]);
    }

    // 3. Send the packet with its header and custom payload
    sendCANDataFrameFunction((sendCANDataFrameHeader){.nodeID = (int32_t)frame->nodeID}, payload, payload_size_bytes);
}