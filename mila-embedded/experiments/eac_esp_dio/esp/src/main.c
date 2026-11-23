/*
 * ESP32 falling-edge timestamp test.
 *
 * Framework: ESP-IDF
 *
 * Intended future input:
 *   EAC-5000 digital output through a verified ESP-safe interface.
 *
 * IMPORTANT:
 *   - Do not connect the EAC circuit to the ESP until voltage,
 *     grounding, transients, and power sequencing are verified.
 *   - The timestamp is taken when the GPIO interrupt handler runs.
 *     It is not a hardware timestamp of the exact physical edge.
 *   - CAN communication is not included in this first test.
 */

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"


/*
 * GPIO34 is a provisional choice.
 *
 * Confirm that GPIO34 is accessible on your ESP board and is not
 * already used by another part of the system.
 *
 * GPIO34 is input-only and does not have an internal pull-up or
 * pull-down. The external signal source must provide a valid level.
 */
#define TEST_INPUT_GPIO GPIO_NUM_34

#define EVENT_QUEUE_LENGTH 128

static const char *TAG = "edge_test";


typedef struct {
    uint32_t sequence;
    int64_t esp_timestamp_us;
} edge_event_t;


static QueueHandle_t edge_queue = NULL;

/*
 * Protects counters shared between the interrupt handler and
 * the application task.
 */
static portMUX_TYPE counter_lock = portMUX_INITIALIZER_UNLOCKED;

static uint32_t next_sequence = 0;
static uint32_t dropped_events = 0;


/*
 * Called when a falling edge is detected.
 *
 * The interrupt service is installed without ESP_INTR_FLAG_IRAM
 * for this first test. Timing during flash operations still needs
 * to be evaluated for the final design.
 */
static void on_falling_edge(void *arg)
{
    (void)arg;

    edge_event_t event;

    /*
     * This is the time when the interrupt handler runs, which may
     * be later than the physical edge.
     */
    event.esp_timestamp_us = esp_timer_get_time();

    portENTER_CRITICAL_ISR(&counter_lock);

    event.sequence = next_sequence;
    next_sequence++;

    portEXIT_CRITICAL_ISR(&counter_lock);

    BaseType_t task_woken = pdFALSE;

    if (
        xQueueSendFromISR(
            edge_queue,
            &event,
            &task_woken
        ) != pdPASS
    ) {
        portENTER_CRITICAL_ISR(&counter_lock);

        dropped_events++;

        portEXIT_CRITICAL_ISR(&counter_lock);
    }

    if (task_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}


void app_main(void)
{
    edge_queue = xQueueCreate(
        EVENT_QUEUE_LENGTH,
        sizeof(edge_event_t)
    );

    if (edge_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create the edge-event queue");
        return;
    }

    gpio_config_t input_config = {
        .pin_bit_mask = 1ULL << TEST_INPUT_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };

    ESP_ERROR_CHECK(gpio_config(&input_config));

    /*
     * This standalone test assumes no other code has installed the
     * GPIO interrupt service.
     *
     * When integrating into the existing vehicle application, check
     * whether that application already installed the service.
     */
    ESP_ERROR_CHECK(gpio_install_isr_service(0));

    ESP_ERROR_CHECK(
        gpio_isr_handler_add(
            TEST_INPUT_GPIO,
            on_falling_edge,
            NULL
        )
    );

    ESP_LOGI(
        TAG,
        "Listening for falling edges on GPIO %d",
        TEST_INPUT_GPIO
    );

    ESP_LOGI(
        TAG,
        "Timestamps are ESP elapsed time in microseconds"
    );

    printf("sequence,esp_timestamp_us,dropped_events\n");
    fflush(stdout);

    while (1) {
        edge_event_t event;

        if (
            xQueueReceive(
                edge_queue,
                &event,
                portMAX_DELAY
            ) == pdPASS
        ) {
            uint32_t drops;

            portENTER_CRITICAL(&counter_lock);

            drops = dropped_events;

            portEXIT_CRITICAL(&counter_lock);

            printf(
                "%" PRIu32 ",%" PRId64 ",%" PRIu32 "\n",
                event.sequence,
                event.esp_timestamp_us,
                drops
            );

            fflush(stdout);
        }
    }
}