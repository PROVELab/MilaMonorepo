#include "sensorHelper.hpp"
#include "../../pecan/pecan.h"
#include "../../programConstants.h"
// recomended for viewing this file, select one env for which SENSOR_ESP_BUILD is defined (like genericNodeNameESP)
//, and then switch to one with SENSOR_ARDUINO_BUILD defined (like genericNodeName)
// this file contains lots of code specific to each platform, but enough similar code to make it worth keeping as one
// file

#ifdef SENSOR_ESP_BUILD
#include "../../espBase/debug_esp.h" //sets uo static mutexes. To add another mutex, declare it in this file, and its .c file, and increment mutexCount
#include "driver/gpio.h"
#include "driver/twai.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stdbool.h>
#include <string.h>
// Declare Timers for data collection and sending
TimerHandle_t dataCollection_Timers[numFrames]; // one of these timers going off trigers callback function for missing
                                                // CAN Data Frane
StaticTimer_t xTimerBuffers[numFrames];         // array for the buffers of these timers
// task for monitoring bus status, and restarting if needed
#define SENSOR_STACK_SIZE 1000 // checkBus task is reliabley small, so we can use a smaller stack size
StaticTask_t checkBus_Buffer;
StackType_t checkBus_Stack[SENSOR_STACK_SIZE]; // buffer that the task will use as its stack
int32_t checkBus_myId = myId;                  // parameter passed to check_bus_status task
#elif defined(SENSOR_ARDUINO_BUILD)
#include "../../arduinoSched/arduinoSched.hpp"
#include "Arduino.h"
#include "CAN.h"
#endif

static const char* TAG = "SensorHelper";

#include <stdint.h>

/*Note: myframes is an extern variable declared in sensorHelper.hpp, and defined in <nodeName>/sensorStaticDec.cpp.
 This variable is needed to format and send CAN Data. While with the default implementation this variable is only needed
 in this file, however it is possible that sensor-specific code will want to access it in the future, which is why its
 defined externally*/

#ifdef SENSOR_ESP_BUILD
int8_t dataIndices[numFrames] = {0}; // initialized to 0, 1, 2, 3,... in vitalsInit, used to store sendFrame Indices
void espSendFrame(TimerHandle_t xTimer) {
    const int8_t frameNum = *((int8_t*) (pvTimerGetTimerID(xTimer)));
    sendFrame(frameNum);
}
#endif

#ifdef SENSOR_ARDUINO_BUILD // On arduino task library, task functions cant have parameters, using templates to define a
                            // distinct function for collecting each frame
void (*sendFrameHandlers[numFrames])(void); // each of these functions collects data for, and sends a CAN frame
template <int N> void func() { sendFrame(N); }

// Recursively defines func<1>, func<2>, ... func<numFrames>, and sets the handler[i]=func<i>, ei, initializes callbacks
// for sendFrame tasks for any arbitrary value of numFrames.
template <int N> struct GenerateFunctions {
    static void generate(void (*sendFrameHandlers[])(void)) {
        sendFrameHandlers[N - 1] = &func<N - 1>;               // Assign func<N-1> to the array
        GenerateFunctions<N - 1>::generate(sendFrameHandlers); // Recurse
    }
};
// Base template  (for N-1 = 0)
template <> struct GenerateFunctions<1> {
    static void generate(void (*sendFrameHandlers[])(void)) {
        sendFrameHandlers[0] = &func<0>; // Base case: assign func<0>()
    }
};
#endif

// vitals Compliance, expects Can to alr be initialized.
//^Creates listen param for heartbeats, and creates tasks for collecting data, and Bus State Monitoring
int8_t sensorInit(PCANListenParamsCollection* plpc,
                  void* ts) { // void* ts = PScheduler for arduino, may be NULL otherwise
#ifdef SENSOR_ESP_BUILD
    ESP_LOGI(TAG, "initializing");
#endif

#ifdef SENSOR_ESP_BUILD
    for (int i = 0; i < numFrames; i++) { // create timers for sendingData
        dataIndices[i] = i;               // initialize indices for frames.
        dataCollection_Timers[i] = xTimerCreateStatic(
            "Timer",                              // Just a text name, not used by the RTOS kernel.
            pdMS_TO_TICKS(myframes[i].period), // The timer period in ticks, must be greater than 0.
            pdTRUE,                               // The timers will auto-reload themselves when they expire.
            (void*) &(dataIndices[i]), //"ID" for this function, which we use to store the corresponding Can Frame for
                                       // this timer
            espSendFrame,                 /* callBack functoin, sends corresponding frame */
            &(xTimerBuffers[i]) // Pass in the address of a StaticTimer_t variable, which will hold the data associated
                                // with the timer being created.
        );
    }
    // start the timers:
    for (int i = 0; i < numFrames; i++) {
        if (xTimerStart(dataCollection_Timers[i], pdMS_TO_TICKS(1000)) == pdFAIL) {
            ESP_LOGE(TAG, "unable to start a timer");
            while (1);
        }
    }

#elif defined(SENSOR_ARDUINO_BUILD)
    PScheduler* sched = (PScheduler*) ts;
    GenerateFunctions<numFrames>::generate(sendFrameHandlers);
    // schedules dataCollection + frame Sends:
    for (int i = 0; i < numFrames; i++) {
        sched->scheduleTask(sendFrameHandlers[i], myframes[i].period);
        Serial.print("scheduling");
    }
#endif
    vitalsInit(plpc, myId); // creates listen param for heartbeats
    // send init status update

    return 0;
}
