#include <stdint.h>

#include "esp_log.h"
#include "esp_timer.h"

#include "DHT.hpp"
#include "temp_humid.h"

#define DHTDataLine GPIO_NUM_33

static const char TAG[] = "temp_humid";

DHT dht;

extern "C" void initTempHumid(){
    esp_rom_gpio_pad_select_gpio(DHTDataLine);
    dht.setDHTgpio(DHTDataLine);
}

static void checkRefresh(){
    static constexpr int64_t FRESHNESS_TIMEOUT_MS = 2500; //must have at least 2s between querying the device
    static int64_t last_update_time_us = -FRESHNESS_TIMEOUT_MS * 1000;

    if ((esp_timer_get_time() - last_update_time_us) < (FRESHNESS_TIMEOUT_MS * 1000)) {
        return;  //last read was within FRESHNESS_TIMEOUT ago
    }
    //otherwise, refresh and update time
    int ret = dht.readDHT();
    dht.errorHandler(ret);
    last_update_time_us = esp_timer_get_time();
}


extern "C" int32_t getTempF(){
    checkRefresh();

    const float temp_C = dht.getTemperature();
    const float temp_F = (temp_C * 1.8f) + 32.0f;
    return (int32_t)temp_F;
}


extern "C" int32_t getRH(){
    checkRefresh();

    return (int32_t) dht.getRH();
}