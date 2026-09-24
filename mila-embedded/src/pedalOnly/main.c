#include <inttypes.h>
#include <stdio.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "../sensors/pedalSensor/pedalInterpolation.h"
#include "../sensors/powerSensor/powerSensor.h"

#define numADCChannels 3

typedef enum { pedalPower_Index = 0, reading1_Index = 1, reading2_Index = 2 } ADC_Indices;

static const char* TAG = "pedalOnly";

void app_main(void) {
    constexpr int large_R = 100000;
    constexpr int small_R = 64900;
    // selfPowerConfig channels[numADCChannels] = {
    //     {.ADCPin = VP_Pin, .R1 = large_R, .R2 = large_R},
    //     {.ADCPin = VN_Pin, .R1 = small_R, .R2 = large_R},
    //     {.ADCPin = 35, .R1 = small_R, .R2 = large_R},
    // };


        // 1) Build per-channel configs (order matters)
    selfPowerConfig channels[numADCChannels] = {{.ADCPin = VN_Pin, .R1 = large_R, .R2 = large_R},  // Vcc
                                                {.ADCPin = 35, .R1 = small_R, .R2 = large_R},     // rising (white wire)
                                                {.ADCPin = 34, .R1 = small_R, .R2 = large_R},     // falling (red wire)
                                                // {.ADCPin = VP_Pin, .R1 = 1, .R2 = large_R}     //brake
                                            };      

    selfPowerStatus_t init_status[numADCChannels];
    initializeSelfPower(channels, numADCChannels, 1, init_status);

    for (;;) {
        int32_t readings[numADCChannels];
        selfPowerStatus_t statuses[numADCChannels];
        collectSelfPowerAllmV(readings, statuses);

        if (statuses[pedalPower_Index] == READ_SUCCESS && readings[pedalPower_Index] >= 4500 &&
            readings[pedalPower_Index] <= 7500 && statuses[reading1_Index] == READ_SUCCESS &&
            statuses[reading2_Index] == READ_SUCCESS) {
            ESP_LOGI(TAG, "power=%" PRId32 "mV rising=%" PRId32 "mV (%" PRId32
                          "%%) falling=%" PRId32 "mV (%" PRId32 "%%)",
                     readings[pedalPower_Index], readings[reading1_Index],
                     transformPedalReading(readings[reading1_Index], readings[pedalPower_Index], risingPedalIndex),
                     readings[reading2_Index],
                     transformPedalReading(readings[reading2_Index], readings[pedalPower_Index], fallingPedalIndex));
        } else {
            ESP_LOGW(TAG, "read failed: power=%" PRId32 " (%d), rising=%" PRId32 " (%d), falling=%" PRId32
                          " (%d)",
                     readings[pedalPower_Index], statuses[pedalPower_Index], readings[reading1_Index],
                     statuses[reading1_Index], readings[reading2_Index], statuses[reading2_Index]);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
