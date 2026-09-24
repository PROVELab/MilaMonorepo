#include "LEDShow.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" void app_main(void)
{
    LEDShow.begin();
    LEDShow.setBrightness(10);

    // A directly specified RGB rainbow.  Each frame moves its colors one LED
    // forward, so the rainbow appears to shuffle along the strip.
    constexpr CRGB rainbow[] = {
        {255,   0,   0}, // red
        {255, 127,   0}, // orange
        {255, 255,   0}, // yellow
        {  0, 255,   0}, // green
        {  0,   0, 255}, // blue
        { 75,   0, 130}, // indigo
        {148,   0, 211}, // violet
    };
    constexpr uint16_t rainbow_size = sizeof(rainbow) / sizeof(rainbow[0]);

    uint16_t offset = 0;
    while (true) {
        for (uint16_t i = 0; i < NUM_LEDS; ++i) {
            leds[i] = rainbow[(i + offset) % rainbow_size];
        }

        LEDShow.show();
        offset = (offset + 1) % rainbow_size;
        vTaskDelay(pdMS_TO_TICKS(75));
    }
}
