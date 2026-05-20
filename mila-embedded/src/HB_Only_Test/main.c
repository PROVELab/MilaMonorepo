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
#include "../programConstants.h"

const char* TAG = "HB_Only_Test";

// Initialize space for each task
StaticTask_t sendHB_Buffer;
StackType_t sendHB_Stack[STACK_SIZE];

StaticTask_t recieveMSG_Buffer;
StackType_t recieveMSG_Stack[STACK_SIZE];

// Initialize space for the terminal task
StaticTask_t terminal_Buffer;
StackType_t terminal_Stack[STACK_SIZE];

void terminalTask(void* pvParameters) {
    for (;;) {
        // fgetc blocks here until you type something in the terminal
        int c = fgetc(stdin); 

        // Ignore empty reads and 'Enter' key carriage returns
        if (c != EOF && c != '\n' && c != '\r') {
            ESP_LOGI(TAG, "Terminal input received: '%c'", c);
            const int enablePrecharge = 4; //leave me alone
            // Handle your single-key commands
            if (c == 'e') {
                CANPacket message = {0};
                setRTR(&message);
                message.id = combinedID(enablePrecharge, vitalsID);
                sendPacket(&message);
                ESP_LOGI(TAG, "enabling precharge!");
            } 
            else if (c == 'd') {
                CANPacket message = {0};
                setRTR(&message);
                message.id = combinedID(2, vitalsID);
                sendPacket(&message);
                ESP_LOGI(TAG, "disabling precharge!");
            } else if (c == 'v'){
                CANPacket message = {0};
                setRTR(&message);
                message.id = combinedID(3, vitalsID);
                sendPacket(&message);
                ESP_LOGI(TAG, "viewing precharge!");
            }
            else {
                ESP_LOGW(TAG, "Unknown command: '%c'", c);
            }
        }
        
        // Small yield to prevent task watchdog starvation
        vTaskDelay(10 / portTICK_PERIOD_MS); 
    }
}

void sendHB(void* pvParameters) {
    for (;;) {
        // Send HB
        CANPacket message = {0};
        setRTR(&message);
        message.id = combinedID(HBPing, vitalsID); // HBPing, vitalsID
        sendPacket(&message);

        ESP_LOGI(TAG, "\n\nsent HB\n\n!");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}


// recv Can messages
void recieveMSG() {
    // an array for matching recieved Can Packet's ID's to their handling functions. MAX length set to 20 by default
    // initialized to default values
    PCANListenParamsCollection plpc = {
        .arr = {{0}},
        .defaultHandler = defaultPacketRecv,
        .size = 0,
    };
    //

    for (;;) { waitPackets(&plpc); }
}
void app_main(void) {
    base_ESP_init();
    pecanInit config = {.nodeId = vitalsID, .pin1 = defaultPin, .pin2 = defaultPin};
    pecan_CanInit(config);

    TaskHandle_t sendHandler =
        xTaskCreateStaticPinnedToCore( // schedules the task to run the printHello function, assigned to core 0
            sendHB,                    /* Function that implements the task. */
            "HeartBeatSend",           /* Text name for the task. */
            STACK_SIZE,                /* Number of indexes in the xStack array. */
            (void*) 1, /* Parameter passed into the task. */ // should only use constants here. Global variables may be
                                                             // ok? cant be a stack variable.
            3,                                               /* Priority at which the task is created. */
            sendHB_Stack,                                    /* Array to use as the task's stack. */
            &sendHB_Buffer,                                  /* Variable to hold the task's data structure. */
            tskNO_AFFINITY);

    TaskHandle_t recieveHandler = xTaskCreateStaticPinnedToCore( // recieves CAN Messages
        recieveMSG,                                              /* Function that implements the task. */
        "msgRecieve",                                            /* Text name for the task. */
        STACK_SIZE,                                              /* Number of indexes in the xStack array. */
        (void*) 1, /* Parameter passed into the task. */ // should only use constants here. Global variables may be ok?
                                                         // cant be a stack variable.
        tskIDLE_PRIORITY,                                /* Priority at which the task is created. */
        recieveMSG_Stack,                                /* Array to use as the task's stack. */
        &recieveMSG_Buffer,                              /* Variable to hold the task's data structure. */
        tskNO_AFFINITY);

    TaskHandle_t terminalHandler = xTaskCreateStaticPinnedToCore(
        terminalTask,          /* Function that implements the task. */
        "TerminalListen",      /* Text name for the task. */
        STACK_SIZE,            /* Number of indexes in the xStack array. */
        (void*) 1,             /* Parameter passed into the task. */
        tskIDLE_PRIORITY + 1,  /* Priority (slightly higher than idle) */
        terminal_Stack,        /* Array to use as the task's stack. */
        &terminal_Buffer,      /* Variable to hold the task's data structure. */
        tskNO_AFFINITY
    );

    }

