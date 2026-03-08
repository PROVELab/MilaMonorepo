#include "tasks.h"

#include "driver/uart.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdint.h>
#include <string.h>

#define VSR_STREAM_UART_NUM        UART_NUM_0 // USB
#define VSR_STREAM_BAUD_RATE       921600
#define VSR_STREAM_PERIOD_MS       100 // 10 Hz
#define VSR_STREAM_PAYLOAD_MAX_LEN vsr_VehicleStatusRegister_size
#define VSR_STREAM_HEADER_LEN      2
#define VSR_STREAM_FRAME_MAX_LEN   (VSR_STREAM_HEADER_LEN + VSR_STREAM_PAYLOAD_MAX_LEN)
#define VSR_STREAM_TX_BUF_LEN      (2 * VSR_STREAM_FRAME_MAX_LEN)

static bool init_vsr_stream_uart(void) {
    const uart_config_t uart_config = {
        .baud_rate = VSR_STREAM_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    // UART0 may already have a driver installed by IDF startup code.
    esp_err_t err = uart_driver_install(VSR_STREAM_UART_NUM, VSR_STREAM_TX_BUF_LEN, 0, 0, NULL, 0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return false;
    }

    if (uart_param_config(VSR_STREAM_UART_NUM, &uart_config) != ESP_OK) {
        return false;
    }

    if (uart_set_pin(VSR_STREAM_UART_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE,
                     UART_PIN_NO_CHANGE) != ESP_OK) {
        return false;
    }

    return true;
}

static void uart_write_all(const uint8_t* data, size_t len) {
    size_t offset = 0;
    while (offset < len) {
        int written = uart_write_bytes(VSR_STREAM_UART_NUM, (const char*) data + offset, len - offset);
        if (written <= 0) {
            break;
        }
        offset += (size_t) written;
    }
}

static void vsr_stream_main(void* arg) {
    (void) arg;

    if (!init_vsr_stream_uart()) {
        vTaskDelete(NULL);
        return;
    }

    const vehicle_status_reg_t* vsr = (const vehicle_status_reg_t*) &vsr_global;
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(VSR_STREAM_PERIOD_MS);

    uint8_t payload_buf[VSR_STREAM_PAYLOAD_MAX_LEN];
    uint8_t frame_buf[VSR_STREAM_FRAME_MAX_LEN];

    while (1) {
        size_t payload_len = 0;
        if (vsr_serialize(vsr, payload_buf, sizeof(payload_buf), &payload_len) &&
            payload_len <= VSR_STREAM_PAYLOAD_MAX_LEN) {
            // Frame format: [u16 little-endian payload length][protobuf payload bytes].
            frame_buf[0] = (uint8_t) (payload_len & 0xFFu);
            frame_buf[1] = (uint8_t) ((payload_len >> 8) & 0xFFu);
            memcpy(&frame_buf[VSR_STREAM_HEADER_LEN], payload_buf, payload_len);

            uart_write_all(frame_buf, payload_len + VSR_STREAM_HEADER_LEN);
        }

        xTaskDelayUntil(&last_wake_time, period);
    }
}

void start_vsr_stream_task() {
    static StackType_t vsr_stream_stack[DEFAULT_STACK_SIZE];
    static StaticTask_t vsr_stream_tcb;

    xTaskCreateStaticPinnedToCore(vsr_stream_main, "vsr_stream", DEFAULT_STACK_SIZE, NULL, VSR_STREAM_TASK_PRIO,
                                  vsr_stream_stack, &vsr_stream_tcb, 0);
}
