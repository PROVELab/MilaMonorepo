#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "driver/ledc.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "../sensors/powerSensor/powerSensor.h"
#include <inttypes.h>
#include "driver/gpio.h"
#include "freertos/semphr.h"

// --- Configuration Constants ---
#define PWM_PIN                 25                      // Ideal standard output pin
#define PWM_FREQ_HZ             100                   // max 100kHz for pololu (dont push it tho)
#define PWM_RESOLUTION          LEDC_TIMER_10_BIT       // Lowered to 10-bit to support 25kHz
#define MAX_DUTY_VAL            1023                    // 2^10 - 1
#define FAULT_PIN               14                      // Fault pin from motor driver

#define PWM_TIMER               LEDC_TIMER_0
#define PWM_MODE                LEDC_LOW_SPEED_MODE     // Low speed is perfect for 2kHz
#define PWM_CHANNEL             LEDC_CHANNEL_0

static const char* TAG = "CoolantPWM";

// --- Fault Handling ---
static SemaphoreHandle_t fault_binary;
static StaticSemaphore_t fault_binary_buffer;

// ISR to be called when the fault pin goes low
static void IRAM_ATTR fault_pin_isr_handler(void* arg) {
    xSemaphoreGiveFromISR(fault_binary, NULL);
}

/**
 * @brief Initializes the LEDC timer and channel for PWM output.
 */
void init_pwm(void) {
    // 1. Configure the LEDC Timer
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = PWM_MODE,
        .timer_num        = PWM_TIMER,
        .duty_resolution  = PWM_RESOLUTION,
        .freq_hz          = PWM_FREQ_HZ,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    // 2. Configure the LEDC Channel
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = PWM_MODE,
        .channel        = PWM_CHANNEL,
        .timer_sel      = PWM_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = PWM_PIN,
        .duty           = 0, // Start with duty cycle at 0%
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel);
}

void setDutyCycle(int duty_0_to_100) {
    // Clamp the input to ensure it stays within 0-100
    if (duty_0_to_100 < 0) duty_0_to_100 = 0;
    if (duty_0_to_100 > 100) duty_0_to_100 = 100;

    // Map the 0-100% value to the 10-bit hardware resolution (0 - 1023)
    uint32_t hardware_duty = (duty_0_to_100 * MAX_DUTY_VAL) / 100;

    // Apply the duty cycle to the hardware
    ledc_set_duty(PWM_MODE, PWM_CHANNEL, hardware_duty);
    ledc_update_duty(PWM_MODE, PWM_CHANNEL);
}

static void monitor_current_task(void *pvParameters) {
    const int num_channels = 1;
    int32_t vin_mV[num_channels];
    selfPowerStatus_t statuses[num_channels];

    const int reporting_interval_ms = 1000;
    const int sampling_interval_ms = 10;
    const int samples_per_report = reporting_interval_ms / sampling_interval_ms;

    for (;;) {
        float total_current_mA = 0;
        int sample_count = 0;

        for (int i = 0; i < samples_per_report; i++) {
            collectSelfPowerAllmV(vin_mV, statuses);

            if (statuses[0] == READ_SUCCESS) {
                // The Pololu driver CS pin provides ~20 mV/A.
                // subtract 1.65V, it outputs 1.65V at 0A
                float current_mA = ((float)(vin_mV[0] - 1650)) * 75.8f;
                total_current_mA += current_mA;
                sample_count++;
            } else if (statuses[0] != NOTHING_TO_READ) {
                ESP_LOGW(TAG, "Failed to read current sensor, status: %d", statuses[0]);
            }
            vTaskDelay(pdMS_TO_TICKS(sampling_interval_ms));
        }

        if (sample_count > 0) {
            float avg_current_mA = total_current_mA / sample_count;
            ESP_LOGI(TAG, "Average motor current over last second: %.2f mA", avg_current_mA);
        } else {
            ESP_LOGI(TAG, "No current samples collected in the last second.");
        }
    }
}

static void fault_monitor_task(void *pvParameters) {
    for (;;) {
        if (xSemaphoreTake(fault_binary, portMAX_DELAY) == pdTRUE) {
            ESP_LOGE(TAG, "MOTOR FAULT DETECTED! (GPIO %d went low)", FAULT_PIN);

            while (gpio_get_level(FAULT_PIN) == 0) {
                ESP_LOGE(TAG, "FAULT PERSISTS: Motor driver fault is active.");
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
            ESP_LOGI(TAG, "Fault condition cleared (GPIO %d is high).", FAULT_PIN);
        }
    }
}
// --- Serial Console Task ---
static void console_task(void *pvParameters) {
    char line[128];
    int pos = 0;
    //lowk, i think min 70 is probably the play. it doenst pump muuch <40, and 50 is kinda cringe for the capacitor
    // setDutyCycle((int)70);
    setDutyCycle((int)0);

    for (;;) {
        // Read one character at a time
        int c = fgetc(stdin);

        // EOF usually means nothing is in the buffer right now
        if (c == EOF) {
            vTaskDelay(pdMS_TO_TICKS(20)); // Yield to watchdog
            continue;
        }

        // If we receive a newline or carriage return, the user pressed Enter
        if (c == '\n' || c == '\r') {
            if (pos > 0) {
                line[pos] = '\0'; // Null-terminate the string

                // Parse the string into a long integer
                char *endptr;
                long duty = strtol(line, &endptr, 10);

                // Validation checks
                if (endptr == line || *endptr != '\0' || duty < 0 || duty > 100) {
                    printf("error parsing: %s\n", line);
                } else {
                    ESP_LOGI(TAG, "Setting duty cycle to %ld%%", duty);
                    setDutyCycle((int)duty);
                }
                
                // Reset the buffer index for the next command
                pos = 0; 
            }
        } 
        // Handle Backspace (ASCII 8) or Delete (ASCII 127)
        else if (c == '\b' || c == 127) {
            if (pos > 0) {
                pos--; 
                // Optional: To make backspace visually work in some terminals, 
                // you can echo a backspace, a space, and another backspace.
                // printf("\b \b"); 
                // fflush(stdout);
            }
        } 
        // Otherwise, add the character to the buffer (avoiding overflow)
        else if (pos < sizeof(line) - 1) {
            line[pos++] = (char)c;
            
            // Note: If you can't see what you are typing in the monitor, 
            // uncomment the next two lines to echo characters back to the terminal.
            // putchar(c);
            // fflush(stdout);
        }
    }
}
void app_main(void) {
    // Initialize the PWM hardware
    init_pwm();

    // --- Initialize Fault Pin Monitoring ---
    fault_binary = xSemaphoreCreateBinaryStatic(&fault_binary_buffer);
    if (fault_binary == NULL) {
        ESP_LOGE(TAG, "Failed to create fault semaphore");
    }

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << FAULT_PIN),
        .pull_down_en = 0,
        .pull_up_en = 1,
    };
    gpio_config(&io_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(FAULT_PIN, fault_pin_isr_handler, NULL);

    xTaskCreate(fault_monitor_task, "fault_monitor", 2048, NULL, 10, NULL);
    ESP_LOGI(TAG, "Fault monitoring started on GPIO %d", FAULT_PIN);

    // --- Initialize Current Sensor ---
    const int num_channels = 1;
    selfPowerConfig cs_config = {
        .ADCPin = VP_Pin, 
        .R1 = 10,          
        .R2 = 1000000     
    };
    selfPowerStatus_t statuses[num_channels];
    
    initializeSelfPower(&cs_config, num_channels, 1, statuses);

    if (statuses[0] == INIT_FAILURE) {
        ESP_LOGE(TAG, "Failed to initialize current sensor ADC");
    } else {
        ESP_LOGI(TAG, "Current sensor initialized, calibration status: %d", statuses[0]);
        xTaskCreate(monitor_current_task, "monitor_current", 4096, NULL, 5, NULL);
    }

    // --- Start the Console Task ---
    ESP_LOGI(TAG, "Starting console monitor. Enter a duty cycle (0-100):");
    xTaskCreate(console_task, "console_task", 4096, NULL, 5, NULL);
}