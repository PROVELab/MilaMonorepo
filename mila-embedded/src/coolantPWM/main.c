#include <stdio.h>
#include "driver/ledc.h"
#include "esp_err.h"

// --- Configuration Constants ---
#define PWM_PIN                 18                      // Ideal standard output pin
// #define PWM_FREQ_HZ             2000                    // Configurable frequency (2 kHz)
#define PWM_FREQ_HZ             1000                    // Configurable frequency (2 kHz)

#define PWM_RESOLUTION          LEDC_TIMER_13_BIT       // 13-bit resolution (0 to 8191)
#define MAX_DUTY_VAL            8191                    // 2^13 - 1

#define PWM_TIMER               LEDC_TIMER_0
#define PWM_MODE                LEDC_LOW_SPEED_MODE     // Low speed is perfect for 2kHz
#define PWM_CHANNEL             LEDC_CHANNEL_0

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

    // Map the 0-100% value to the 13-bit hardware resolution (0 - 8191)
    uint32_t hardware_duty = (duty_0_to_100 * MAX_DUTY_VAL) / 100;

    // Apply the duty cycle to the hardware
    ledc_set_duty(PWM_MODE, PWM_CHANNEL, hardware_duty);
    ledc_update_duty(PWM_MODE, PWM_CHANNEL);
}

void app_main(void) {
    // Initialize the PWM hardware
    init_pwm();

    // Example usage:
    printf("Setting duty cycle to 25%%\n");
    setDutyCycle(50);
    
    // vTaskDelay(pdMS_TO_TICKS(2000));
    
    // printf("Setting duty cycle to 75%%\n");
    // setDutyCycle(75);
}