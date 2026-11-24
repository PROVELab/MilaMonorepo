/*
 * ESP32-to-EAC digital-input test.
 *
 * GPIO25 produces a slow 1 Hz signal for the MOSFET gate.
 * ESP-IDF; standalone test firmware.
 *
 * Timestamps describe when software commands the GPIO. They are
 * not measurements of the physical edge at the EAC input.
 */

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TEST_OUTPUT_GPIO GPIO_NUM_25
#define HALF_PERIOD_MS 500

static const char *TAG = "esp_output_test";

void app_main(void)
{
    gpio_config_t output_config = {
        .pin_bit_mask = 1ULL << TEST_OUTPUT_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&output_config));
    ESP_ERROR_CHECK(gpio_set_level(TEST_OUTPUT_GPIO, 0));

    ESP_LOGI(TAG, "Toggling GPIO25 at approximately 1 Hz");
    ESP_LOGI(TAG, "HIGH requests MOSFET on; LOW requests MOSFET off");

    printf("sequence,command,esp_before_us,esp_after_us\n");
    fflush(stdout);

    uint32_t sequence = 0;

    while (1) {
        int64_t before_us = esp_timer_get_time();
        ESP_ERROR_CHECK(gpio_set_level(TEST_OUTPUT_GPIO, 1));
        int64_t after_us = esp_timer_get_time();

        printf(
            "%" PRIu32 ",HIGH,%" PRId64 ",%" PRId64 "\n",
            sequence++,
            before_us,
            after_us
        );
        fflush(stdout);

        vTaskDelay(pdMS_TO_TICKS(HALF_PERIOD_MS));

        before_us = esp_timer_get_time();
        ESP_ERROR_CHECK(gpio_set_level(TEST_OUTPUT_GPIO, 0));
        after_us = esp_timer_get_time();

        printf(
            "%" PRIu32 ",LOW,%" PRId64 ",%" PRId64 "\n",
            sequence++,
            before_us,
            after_us
        );
        fflush(stdout);

        vTaskDelay(pdMS_TO_TICKS(HALF_PERIOD_MS));
    }
}