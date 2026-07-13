#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"

// --- Configuration ---
// Connect these directly to the A/B lines of the target module
#define PIN_A           GPIO_NUM_17  // "Positive" or D+ line
#define PIN_B           GPIO_NUM_16  // "Negative" or D- line

#define BAUD_RATE       56000

// Invert this if your A/B are swapped (try false first, then true if no response)
#define SWAP_POLARITY   false 

// --- Timing Calculations ---
// We use CPU cycles for precision because 56k is fast
static uint32_t cycles_per_bit;

void setup_bitbang_timing() {
    uint32_t cpu_freq_hz = esp_rom_get_cpu_ticks_per_us() * 1000000;
    cycles_per_bit = cpu_freq_hz / BAUD_RATE;
}

// Helper to set differential voltage
// Logic 1 (Mark/Idle): A=1, B=0
// Logic 0 (Space):     A=0, B=1
static inline void set_bus_state(int logic_level) {
    if (SWAP_POLARITY) logic_level = !logic_level;

    if (logic_level == 1) {
        gpio_set_level(PIN_A, 1);
        gpio_set_level(PIN_B, 0);
    } else {
        gpio_set_level(PIN_A, 0);
        gpio_set_level(PIN_B, 1);
    }
}

void send_message_bitbang(const uint8_t *data, int len) {
    // 1. Setup Outputs
    gpio_set_direction(PIN_A, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_B, GPIO_MODE_OUTPUT);
    
    // Idle state (Logic 1)
    set_bus_state(1);
    esp_rom_delay_us(1000); // Hold idle for a moment before starting

    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

    for (int i = 0; i < len; i++) {
        uint8_t byte = data[i];

        // CRITICAL SECTION: Disable interrupts to prevent jitter
        portENTER_CRITICAL(&mux);
        
        uint32_t start_cycle = esp_cpu_get_cycle_count();

        // --- Start Bit (Logic 0) ---
        set_bus_state(0);
        while ((esp_cpu_get_cycle_count() - start_cycle) < cycles_per_bit);
        start_cycle += cycles_per_bit;

        // --- Data Bits (LSB First) ---
        for (int b = 0; b < 8; b++) {
            if (byte & (1 << b)) {
                set_bus_state(1);
            } else {
                set_bus_state(0);
            }
            // Wait for exactly one bit period
            while ((esp_cpu_get_cycle_count() - start_cycle) < cycles_per_bit);
            start_cycle += cycles_per_bit;
        }

        // --- Stop Bit (Logic 1) ---
        set_bus_state(1);
        while ((esp_cpu_get_cycle_count() - start_cycle) < cycles_per_bit);
        
        // End of byte. 
        portEXIT_CRITICAL(&mux);
        
        // Small inter-byte delay (optional, but safer for slow receivers)
        // esp_rom_delay_us(10); 
    }

    // 2. Release the Bus (High-Z)
    // Switch pins to Input so the other side can drive them
    gpio_set_direction(PIN_A, GPIO_MODE_INPUT);
    gpio_set_direction(PIN_B, GPIO_MODE_INPUT);
    
    printf("Message sent. Bus released to High-Z.\n");
}

void app_main(void)
{
    // Initialize GPIOs
    gpio_reset_pin(PIN_A);
    gpio_reset_pin(PIN_B);
    setup_bitbang_timing();

    // The message
    const uint8_t message[] = { 
        0x55, 0x10, 0x01, 0x05, 0x2A, 
        0x49, 0x44, 0x4E, 0x3F, 0xA6, 
        0x2B, 0xAA 
    };

    while (1) {
        printf("Sending bit-banged differential message...\n");
        send_message_bitbang(message, sizeof(message));
        
        // Wait 3 seconds before trying again
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}