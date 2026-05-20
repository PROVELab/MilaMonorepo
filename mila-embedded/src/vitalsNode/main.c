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
#include "../programConstants.h"  //Constants
// random vitals stuff:
#include "vitalsData.h"
#include "vitalsHB.h"
#include "vitalsHelper/vitalsHelper.h"
#include "vitalsHelper/vitalsPacketSendLUT.h"
#include "vitalsHelper/vitalsStaticDec.h"
#include "vitalsLoraRecv.hpp"
#include "../LoraCommon/LoraProtocol.h"

// Initialize space for each task
#define STACK_SIZE 8000 //gemini i stg do not rmeove this! stop!
static const char* TAG = "VitalsMain";
StaticTask_t sendHB_Buffer;
StackType_t sendHB_Stack[STACK_SIZE];
StaticTask_t recieveMSG_Buffer;
StackType_t recieveMSG_Stack[STACK_SIZE];
StaticTask_t checkStatus_Buffer;
StackType_t checkStatus_Stack[STACK_SIZE];

StaticTask_t LORA_Read_Buffer;
StackType_t LORA_Read_Stack[STACK_SIZE];

// send bus status info to telem, also prints it out
void vitals_check_bus_status(void* pvParameters) {
    for (;;) {
        twai_status_info_t status_info;
        if (twai_get_status_info(&status_info) == ESP_OK) {
            ESP_LOGD(TAG,
                     "Bus Status - TXQ:%lu, RXQ:%lu, TX_err:%lu, RX_err:%lu, TX_fail:%lu, RX_miss:%lu, "
                     "RX_overrun:%lu, ARB_lost:%lu, BUS_err:%lu",
                     status_info.msgs_to_tx, status_info.msgs_to_rx, status_info.tx_error_counter,
                     status_info.rx_error_counter, status_info.tx_failed_count, status_info.rx_missed_count,
                     status_info.rx_overrun_count, status_info.arb_lost_count, status_info.bus_error_count);

            static uint32_t errCnt = 0, txFails = 0, rxOverrun = 0, rxMissed = 0; // records previous value

            // Positional initialization for sendBusStatusArgs
            sendBusStatusArgs bus_status_args = {
                0, // mask (will be overwritten by sendBusStatusFunction)
                {status_info.state},
                status_info.tx_error_counter,
                status_info.rx_error_counter,
                status_info.bus_error_count - errCnt,
                status_info.tx_failed_count - txFails,
                status_info.rx_overrun_count - rxOverrun,
                status_info.rx_missed_count - rxMissed,
                status_info.msgs_to_rx
            };
            sendBusStatusFunction(bus_status_args);

            // Update static counters for next delta calculation
            errCnt = status_info.bus_error_count; // record current value for next time
            txFails = status_info.tx_failed_count;
            rxOverrun = status_info.rx_overrun_count;
            rxMissed = status_info.rx_missed_count;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// recv Can messages
void recieveMSG() {
    // an array for matching recieved Can Packet's ID's to their handling functions. MAX length set to 20 by default
    // initialized to default values
    PCANListenParamsCollection plpc = { // Positional initialization
        {{0}}, // arr (initializes all elements to zero)
        defaultPacketRecv,
        0
    };

    // HB process listen Param
    CANListenParam processBeat;
    processBeat.handler = recieveHeartbeat;
    processBeat.listen_id = combinedID(HBPong, vitalsID); // setting vitals ID doesnt matter, just checking function
    processBeat.mt = MATCH_FUNCTION; // MATCH_EXACT to make id and function code require match. MATCH_ID for same 7 bits
                                     // of node ID. MATCH_FUNCTION for same 4 bits of function code
    if (addParam(&plpc, processBeat) != SUCCESS) { // adds the parameter
        ESP_LOGE(TAG, "plpc no room for HB handler");
        while (1);
    }
    //

    // Data process listen Param
    initializeDataTimers(); // initialize timers needed to monitor data
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
    //

    for (;;) { waitPackets(&plpc); }
}

void app_main(void) {
    vTaskDelay(pdMS_TO_TICKS(2000));  //chillax

    xTaskCreateStatic(loraRecvTask, "Lora_Send_Task", STACK_SIZE, NULL, 1, LORA_Read_Stack, &LORA_Read_Buffer);
    while(!LoraDriverRunning()) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGI(TAG, "Waiting for Lora Driver to start...");
    }

    pecanInit config = {.nodeId = vitalsID, .pin1 = defaultPin, .pin2 = defaultPin};
    pecan_CanInit(config); // schedules the task to run the printHello function, assigned to core 0
    xTaskCreateStaticPinnedToCore(
            sendHB,                    /* Function that implements the task. */
            "HeartBeatSend",           /* Text name for the task. */
            STACK_SIZE,                /* Number of indexes in the xStack array. */
            (void*) 1, /* Parameter passed into the task. */ // should only use constants here. Global variables may be
                                                             // ok? cant be a stack variable.
            3,                                               /* Priority at which the task is created. */
            sendHB_Stack,                                    /* Array to use as the task's stack. */
            &sendHB_Buffer,                                  /* Variable to hold the task's data structure. */
            tskNO_AFFINITY);

    xTaskCreateStaticPinnedToCore( // recieves CAN Messages
        recieveMSG,                                              /* Function that implements the task. */
        "msgRecieve",                                            /* Text name for the task. */
        STACK_SIZE,                                              /* Number of indexes in the xStack array. */
        (void*) 1, /* Parameter passed into the task. */ // should only use constants here. Global variables may be ok?
                                                         // cant be a stack variable.
        tskIDLE_PRIORITY,                                /* Priority at which the task is created. */
        recieveMSG_Stack,                                /* Array to use as the task's stack. */
        &recieveMSG_Buffer,                              /* Variable to hold the task's data structure. */
        tskNO_AFFINITY);

    xTaskCreateStaticPinnedToCore( // prints out bus status info
        vitals_check_bus_status,                                  /* Function that implements the task. */
        "checkCan",                                               /* Text name for the task. */
        STACK_SIZE,                                               /* Number of indexes in the xStack array. */
        (void*) 1, /* Parameter passed into the task. */ // should only use constants here. Global variables may be ok?
                                                         // cant be a stack variable.
        1,                                               /* Priority at which the task is created. */
        checkStatus_Stack,                               /* Array to use as the task's stack. */
        &checkStatus_Buffer,                             /* Variable to hold the task's data structure. */
        tskNO_AFFINITY);
}
