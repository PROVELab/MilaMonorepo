#include "driver/gpio.h"
#include "driver/twai.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "esp_log.h"
#include "../pecan/pecan.h"       //helper code for CAN stuff
#include "../programConstants.h"  //Constants
// random vitals stuff:
#include "vitalsData/vitalsData.h"
#include "vitalsHB/vitalsHB.h"
#include "vitalsHelper/vitalsHelper.h"
#include "vitalsGen/vitalsPacketSendLUT.h"
#include "vitalsGen/vitalsStructs.h"
#include "loraMain/vitalsLora.h"
#include "../LoraCommon/LoraProtocol.h"

#include "contactorControl.h"

// Initialize space for each task
#define STACK_SIZE 8000 
static const char* TAG = "VitalsMain";
StaticTask_t receiveMSG_Buffer;
StackType_t receiveMSG_Stack[STACK_SIZE];

//all vitals' task priorities
const UBaseType_t recvCAN_Priority = tskIDLE_PRIORITY;
const UBaseType_t send_HB_Priority = 3;
const UBaseType_t data_process_Priority = 4;
const UBaseType_t check_HB_Priority = 5;
const UBaseType_t LoraRecv_Priority = 6;
const UBaseType_t LoraMonitor_Priority = 7;

// recv Can messages
void receiveMSG(void* pvParameters) {
    PCANListenParamsCollection plpc = { 
        {{0}}, 
        vitals_defaultPacketRecv,
        0
    };

    //for HeartBeats
    CANListenParam processBeat;
    processBeat.handler = receiveHeartbeat;
    processBeat.listen_id = combinedID(HBPong, vitalsID); // setting vitals ID doesnt matter, just checking function
    processBeat.mt = MATCH_FUNCTION; // MATCH_EXACT to make id and function code require match. MATCH_ID for same 7 bits
                                     // of node ID. MATCH_FUNCTION for same 4 bits of function code
    if (addParam(&plpc, processBeat) != SUCCESS) { // adds the parameter
        ESP_LOGE(TAG, "plpc no room for HB handler");
        while (1);
    }

    //for processing data
    CANListenParam processData;
    processData.handler = monitorData;
    processData.listen_id =
    combinedID(transmitData, vitalsID); // setting vitals ID doesnt matter, just checking function
    processData.mt = MATCH_FUNCTION; // MATCH_EXACT to make id and function code require match. MATCH_ID for same 7 bits
                                     // of node ID. MATCH_FUNCTION for same 4 bits of function code
    if (addParam(&plpc, processData) != SUCCESS) { // adds the parameter
        ESP_LOGE(TAG, "plpc no room for data handler");
        while (1);
    }

    // for status updates
    CANListenParam processStatusUpdate;
    processStatusUpdate.handler = forwardStatusUpdate;
    processStatusUpdate.listen_id = combinedID(statusUpdate, 0); // Match any node ID
    processStatusUpdate.mt = MATCH_FUNCTION;
    if (addParam(&plpc, processStatusUpdate) != SUCCESS) {
        ESP_LOGE(TAG, "plpc no room for status update handler");
        while(1);
    }

    for (;;) { waitPackets(&plpc); }
}

void app_main(void) {
    vTaskDelay(pdMS_TO_TICKS(2000));  //chillax

    pecanInit config = {.nodeId = vitalsID, .pin1 = defaultPin, .pin2 = defaultPin};


    vitalsLoraInit(LoraRecv_Priority, LoraMonitor_Priority); //blocks until initialized
    pecan_CanInit(config); // schedules the task to run the printHello function, assigned to core 0
    HBInit(send_HB_Priority, check_HB_Priority);
    vitalsDataInit(data_process_Priority);
    contactorControlInit();

    xTaskCreateStaticPinnedToCore( // receives CAN Messages
        receiveMSG,                     /* Function that implements the task. */
        "msgreceive",                   /* Text name for the task. */
        STACK_SIZE,                     /* Number of indexes in the xStack array. */
        (void*) 1,                      /* Parameter passed into the task. */ 
        recvCAN_Priority,               /* Priority at which the task is created. */
        receiveMSG_Stack,               /* Array to use as the task's stack. */
        &receiveMSG_Buffer,             /* Variable to hold the task's data structure. */
        tskNO_AFFINITY);
}
