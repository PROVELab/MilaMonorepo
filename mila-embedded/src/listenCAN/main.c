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

#include "../espBase/debug_esp.h" //for checking and restarting CAN bus
#include "../pecan/pecan.h"       //helper code for CAN stuff
#include "../programConstants.h"  //Constants

StaticTask_t receiveMSG_Buffer;
StackType_t receiveMSG_Stack[STACK_SIZE];

// recv Can messages
void receiveMSG(void* pvParameters) {
    // an array for matching received Can Packet's ID's to their handling functions. MAX length set to 20 by default
    // initialized to default values
    PCANListenParamsCollection plpc = {
        .arr = {{0}},
        .defaultHandler = defaultPacketRecv,
        .size = 0,
    };

    for (;;) { waitPackets(&plpc); 
    ESP_LOGI("main", "loop");
    }
}

void app_main(void) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    base_ESP_init();
    pecanInit config = {.nodeId = vitalsID, .pin1 = defaultPin, .pin2 = defaultPin};
    pecan_CanInit(config);
    xTaskCreateStaticPinnedToCore( // receives CAN Messages
        receiveMSG,                                              /* Function that implements the task. */
        "msgreceive",                                            /* Text name for the task. */
        STACK_SIZE,                                              /* Number of indexes in the xStack array. */
        (void*) 1, /* Parameter passed into the task. */ // should only use constants here. Global variables may be ok?
                                                         // cant be a stack variable.
        tskIDLE_PRIORITY,                                /* Priority at which the task is created. */
        receiveMSG_Stack,                                /* Array to use as the task's stack. */
        &receiveMSG_Buffer,                              /* Variable to hold the task's data structure. */
        tskNO_AFFINITY);
}