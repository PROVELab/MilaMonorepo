#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "driver/twai.h"
#include "freertos/semphr.h"
#include <string.h>
#include "esp_timer.h"

#include "esp_log.h"

#include "DHT.hpp"

#define DHTDataLine GPIO_NUM_33

#define STACK_SIZE 2000
//add declerations to allocate space for additional tasks here as needed
StaticTask_t collectHumidity_Buffer;
StackType_t collectHumidity_Stack[STACK_SIZE]; //buffer that the task will use as its stack

static const char TAG[] = "DHT";

void DHT_task(void *pvParameter)
{
    DHT dht;
    dht.setDHTgpio(DHTDataLine);
    ESP_LOGI(TAG, "Starting DHT Task\n\n");

    while (1)
    {
        int ret = dht.readDHT();
		
        dht.errorHandler(ret);
		float rel_humidity = dht.getRH();
		float temp_C = dht.getTemperature();
		float absolute_humidity =  DHT::getAbsoluteHumidity(temp_C, rel_humidity); //g/m^3$

        ESP_LOGI(TAG, "rel Hum: %.1f Tmp (Celsius): %.1f, abs Hum (g/m^3): %.1f\n", rel_humidity, temp_C, absolute_humidity);

        // -- wait at least 2 sec before reading again ------------
        // The interval of whole process must be beyond 2 seconds !!
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}

extern "C" void app_main() {

    esp_rom_gpio_pad_select_gpio(DHTDataLine);

    TaskHandle_t temp_humid_handler = xTaskCreateStaticPinnedToCore( // prints out bus status info
        DHT_task,                                  /* Function that implements the task. */
        "getHumidity",                                               /* Text name for the task. */
        STACK_SIZE,                                               /* Number of indexes in the xStack array. */
        (void*) 1, /* Parameter passed into the task. */ // should only use constants here. No stack variables
        1,                                               /* Priority at which the task is created. */
        collectHumidity_Stack,                               /* Array to use as the task's stack. */
        &collectHumidity_Buffer,                             /* Variable to hold the task's data structure. */
        tskNO_AFFINITY);
}