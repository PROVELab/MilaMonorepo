#include "coolant.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "driver/ledc.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include <stdatomic.h> // Include C11 Atomics
#include "../powerSensor/powerSensor.h" // Adjust path if necessary

// --- Configuration Constants ---
#define PWM_PIN                 25
#define PWM_FREQ_HZ             100                   
#define PWM_RESOLUTION          LEDC_TIMER_10_BIT       
#define MAX_DUTY_VAL            1023                    
#define FAULT_PIN               14                      

#define PWM_TIMER               LEDC_TIMER_0
#define PWM_MODE                LEDC_LOW_SPEED_MODE     
#define PWM_CHANNEL             LEDC_CHANNEL_0

static const char* TAG = "CoolantNode";

static _Atomic int32_t coolant_duty_cycle = 0;
static _Atomic int32_t coolant_fault_active = 0;
static _Atomic int32_t coolant_freq_khz = (PWM_FREQ_HZ / 1000); 
static _Atomic int32_t coolant_avg_current_mA = 0;
static _Atomic int32_t coolant_peak_current_mA = 0;

// --- Getter Implementations ---
int32_t get_coolant_duty_cycle(void) { return atomic_load(&coolant_duty_cycle); }
int32_t get_coolant_fault_active(void) { return atomic_load(&coolant_fault_active); }
int32_t get_coolant_freq_khz(void) { return atomic_load(&coolant_freq_khz); }
int32_t get_coolant_avg_current_mA(void) { return atomic_load(&coolant_avg_current_mA); }
int32_t get_coolant_peak_current_mA(void) { return atomic_load(&coolant_peak_current_mA); }

// --- Fault Handling ---
static SemaphoreHandle_t fault_binary;
static StaticSemaphore_t fault_binary_buffer;

static void IRAM_ATTR fault_pin_isr_handler(void* arg) {
    xSemaphoreGiveFromISR(fault_binary, NULL);
}

static void init_pwm(void) {
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = PWM_MODE,
        .timer_num        = PWM_TIMER,
        .duty_resolution  = PWM_RESOLUTION,
        .freq_hz          = PWM_FREQ_HZ,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .speed_mode     = PWM_MODE,
        .channel        = PWM_CHANNEL,
        .timer_sel      = PWM_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = PWM_PIN,
        .duty           = 0, 
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel);
}

void setCoolantDutyCycle(int32_t duty_0_to_100) {
    if (duty_0_to_100 < 0) duty_0_to_100 = 0;
    if (duty_0_to_100 > 100) duty_0_to_100 = 100;

    atomic_store(&coolant_duty_cycle, duty_0_to_100);   

    uint32_t hardware_duty = (duty_0_to_100 * MAX_DUTY_VAL) / 100;
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
        float current_peak_mA = 0;
        int sample_count = 0;

        for (int i = 0; i < samples_per_report; i++) {
            collectSelfPowerAllmV(vin_mV, statuses);

            if (statuses[0] == READ_SUCCESS) {
                float current_mA = ((float)(vin_mV[0] - 1650)) * 75.8f;
                total_current_mA += current_mA;
                
                if (current_mA > current_peak_mA) {
                    current_peak_mA = current_mA;
                }
                sample_count++;
            }
            vTaskDelay(pdMS_TO_TICKS(sampling_interval_ms));
        }

        if (sample_count > 0) {
            atomic_store(&coolant_avg_current_mA, (int32_t)(total_current_mA / sample_count));
            atomic_store(&coolant_peak_current_mA, (int32_t)current_peak_mA);
        }
    }
}

static void fault_monitor_task(void *pvParameters) {
    for (;;) {
        if (xSemaphoreTake(fault_binary, portMAX_DELAY) == pdTRUE) {
            ESP_LOGE(TAG, "MOTOR FAULT DETECTED! (GPIO %d went low)", FAULT_PIN);
            atomic_store(&coolant_fault_active, 1);

            while (gpio_get_level(FAULT_PIN) == 0) {
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
            ESP_LOGI(TAG, "Fault condition cleared (GPIO %d is high).", FAULT_PIN);
            atomic_store(&coolant_fault_active, 0);
        }
    }
}

void init_coolant(void) {
    init_pwm();

    // Fault Monitoring Setup
    fault_binary = xSemaphoreCreateBinaryStatic(&fault_binary_buffer);
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

    // Current Sensor Setup
    const int num_channels = 1;
    selfPowerConfig cs_config = {
        .ADCPin = VP_Pin, // Ensure VP_Pin is available/defined correctly in powerSensor context
        .R1 = 10,          
        .R2 = 1000000     
    };
    selfPowerStatus_t statuses[num_channels];
    
    initializeSelfPower(&cs_config, num_channels, 1, statuses);

    if (statuses[0] == INIT_FAILURE) {
        ESP_LOGE(TAG, "Failed to initialize current sensor ADC");
    } else {
        xTaskCreate(monitor_current_task, "monitor_current", 4096, NULL, 5, NULL);
    }
    
    // Default safe state
    setCoolantDutyCycle(80);
}