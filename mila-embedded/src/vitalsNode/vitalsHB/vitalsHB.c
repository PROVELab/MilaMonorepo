#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "../../pecan/pecan.h" // helper code for CAN stuff
#include "../../programConstants.h"
#include "../vitalsHelper/vitalsHelper.h"
#include "../vitalsGen/vitalsStructs.h"
#include "HBHelper.h"

static const char* TAG = "VitalsHB";

#define STACK_SIZE 8000
//send HB message every second
StaticTask_t sendHB_Buffer;
StackType_t sendHB_Stack[STACK_SIZE];
static void sendHB_Task(void* pvParameters);

//evaluate HB messages 250ms after HB is sent
static void checkHB_Task(void* pvParameters);
StaticTask_t checkHB_Buffer;
StackType_t checkHB_Stack[STACK_SIZE];


static int64_t HBSendTime = 0;
static UBaseType_t checkHBTaskPriority = tskIDLE_PRIORITY;


void HBInit(const UBaseType_t send_HB_Priority, const UBaseType_t check_HB_Priority) {
    initStorage();
    checkHBTaskPriority = check_HB_Priority;

    TaskHandle_t sendHBHandle = xTaskCreateStaticPinnedToCore(
        sendHB_Task,                /* Function that implements the task. */
        "HeartBeatSend",           /* Text name for the task. */
        STACK_SIZE,                /* Number of indexes in the xStack array. */
        (void*) 1,                 /* Parameter passed into the task. */ 
        send_HB_Priority,          /* Priority at which the task is created. */
        sendHB_Stack,              /* Array to use as the task's stack. */
        &sendHB_Buffer,            /* Variable to hold the task's data structure. */
        tskNO_AFFINITY);
    if (sendHBHandle == NULL) {
        ESP_LOGE(TAG, "Failed to create sendHB task");
        while (1);
    }

    ESP_LOGI(TAG, "Heartbeat sender task initialized");

}

static void sendHB_Task(void* pvParameters) {
    ESP_LOGI(TAG, "Heartbeat sender task started");
    // creates the checkHB task
    TaskHandle_t processHBResp = xTaskCreateStaticPinnedToCore( // checksHB responses
        checkHB_Task,                                                /* Function that implements the task. */
        "checkHeartBeatResponses",                              /* Text name for the task. */
        STACK_SIZE,                                             /* Number of indexes in the xStack array. */
        (void*) 1, /* Parameter passed into the task. */ // should only use constants here. Global variables may be ok?
                                                         // cant be a stack variable.
        checkHBTaskPriority,                             /* Priority at which the task is created. */
        checkHB_Stack,                                   /* Array to use as the task's stack. */
        &checkHB_Buffer,                                 /* Variable to hold the task's data structure. */
        tskNO_AFFINITY);                                 // assigns printHello to core 0
    if (processHBResp == NULL) {
        ESP_LOGE(TAG, "Failed to create checkHB task");
        while (1);
    }

    for (;;) {
        // Send HB
        CANPacket message = {0};
        setRTR(&message);
        message.id = combinedID(HBPing, vitalsID); // HBPing, vitalsID
        sendPacket(&message);

        HBSendTime = esp_timer_get_time();
        ESP_LOGI(TAG, "Sent HB");
        vTaskResume(processHBResp); // run task to process HB responses
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

// HB response status, HB timing, and bus status are sent together in VitalsUpdate.
static void checkHB_Task(void* pvParameters) {
    int16_t HBTimesRetriever[numberOfNodes] = {0};

    for (;;) {
        vTaskDelay(250 / portTICK_PERIOD_MS); // give nodes 250ms to respond
        ESP_LOGI(TAG, "Checking HB responses");

        int32_t hb_mask = 0;
        get_and_clear_HB_Values(&hb_mask, HBTimesRetriever);
        format_and_send_HB_info(hb_mask, HBTimesRetriever);

        vTaskSuspend(NULL);
    }
}

int16_t receiveHeartbeat(CANPacket* message) { // mark the HB for given node as received, recording time to respond
    const uint32_t CAN_node_ID = getNodeId((uint32_t)message->id);
    ESP_LOGI(TAG, "Received Pong from: %" PRIu32, CAN_node_ID);
    int16_t nodeIndex = IDTovitalsIndex(CAN_node_ID);
    if (nodeIndex == invalidVitalsIndex) {
        ESP_LOGW(TAG, "Received HB from invalid nodeId %" PRIu32 " (raw CAN id %" PRIi32 "), ignoring", CAN_node_ID, message->id);
        return 0; // invalid id
    }
    // responseTime is in microseconds, scale to milliseconds for a more useful range.
    int64_t responseTime_us = esp_timer_get_time() - HBSendTime;
    int64_t responseTime_ms = responseTime_us / 1000;
    uint16_t responseTime16 = responseTime_ms < 0      ? 0
                              : responseTime_ms > 1023 ? 1023 // Clamp to the 10-bit field max value
                                                       : (uint16_t)responseTime_ms;
    ESP_LOGI(TAG, "response time: %" PRIu16, responseTime16);
    HB_Value_Update(nodeIndex, (int16_t)responseTime16);
    return 0;
}
