#pragma once

#include <stdint.h>

// FastLED-style RGB pixel. Colors in leds[] retain their configured values;
// LEDShow applies brightness only when sending a frame.
struct CRGB {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

constexpr uint16_t NUM_LEDS = 50;

// Configure individual LEDs directly, just as with FastLED:
//   leds[i] = {255, 0, 0};
extern CRGB leds[NUM_LEDS];

class LEDShowController {
public:
    // Initialize the TM1934 output. Call once before show().
    void begin();

    // Set the global output brightness: 0 is off and 255 is full brightness.
    void setBrightness(uint8_t brightness);
    uint8_t getBrightness() const;

    // Scale leds[] using the configured brightness and send one frame.
    void show();

private:
    uint8_t brightness_ = 255;
    void *channel_ = nullptr;
    void *encoder_ = nullptr;
};

// FastLED-style output controller:
//   LEDShow.setBrightness(64);
//   LEDShow.show();
extern LEDShowController LEDShow;
