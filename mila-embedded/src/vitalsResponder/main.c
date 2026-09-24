#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "../espBase/debug_esp.h" //for checking and restarting CAN bus
#include "../pecan/pecan.h"       //helper code for CAN stuff
#include "../programConstants.h"  //Constants

#define RESPONDER_STACK_SIZE 4096

StaticTask_t receiveMSG_Buffer;
StackType_t receiveMSG_Stack[RESPONDER_STACK_SIZE];

// recv Can messages
void receiveMSG(void* pvParameters) {
    // an array for matching received Can Packet's ID's to their handling functions. MAX length set to 20 by default
    // initialized to default values
    PCANListenParamsCollection plpc = {
        .arr = {{0}},
        .defaultHandler = defaultPacketRecv,
        .size = 0,
    };
    vitalsInit(&plpc, 3);

    for (;;) { waitPackets(&plpc); }
}

void app_main(void) {
    base_ESP_init();
    pecanInit config = {.nodeId = vitalsID, .pin1 = defaultPin, .pin2 = defaultPin};
    pecan_CanInit(config);
    xTaskCreateStaticPinnedToCore( // receives CAN Messages
        receiveMSG,                                              /* Function that implements the task. */
        "msgreceive",                                            /* Text name for the task. */
        RESPONDER_STACK_SIZE,                                    /* Number of indexes in the xStack array. */
        (void*) 1, /* Parameter passed into the task. */ // should only use constants here. Global variables may be ok?
                                                         // cant be a stack variable.
        tskIDLE_PRIORITY,                                /* Priority at which the task is created. */
        receiveMSG_Stack,                                /* Array to use as the task's stack. */
        &receiveMSG_Buffer,                              /* Variable to hold the task's data structure. */
        tskNO_AFFINITY);
}
