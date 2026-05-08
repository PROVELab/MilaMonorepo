#include "tasks.h"
#include "vsr_uart_shared.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdint.h>

#define VSR_STREAM_PERIOD_MS       100 // 10 Hz
#define VSR_STREAM_PAYLOAD_MAX_LEN vsr_VehicleStatusRegister_size
#define VSR_STREAM_FRAME_MAX_LEN   (VSR_UART_FRAME_HEADER_LEN + VSR_STREAM_PAYLOAD_MAX_LEN)

static void clear_vsr_log_messages(volatile vehicle_status_reg_t* vsr) {
    if (vsr->log_mutex == NULL) { return; }

    if (xSemaphoreTake(vsr->log_mutex, portMAX_DELAY) == pdTRUE) {
        VSR_DATA.log.message_count = 0;
        xSemaphoreGive(vsr->log_mutex);
    }
}

static void vsr_stream_main(void* arg) {
    (void) arg;

    volatile vehicle_status_reg_t* vsr = &vsr_global;
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(VSR_STREAM_PERIOD_MS);

    uint8_t payload_buf[VSR_STREAM_PAYLOAD_MAX_LEN];
    uint8_t frame_buf[VSR_STREAM_FRAME_MAX_LEN];

    while (1) {
        size_t payload_len = 0;
        size_t frame_len = 0;

        if (vsr_serialize((const vehicle_status_reg_t*) vsr, payload_buf, sizeof(payload_buf), &payload_len) &&
            vsr_build_framed_packet(payload_buf, payload_len, frame_buf, sizeof(frame_buf), &frame_len)) {
            vsr_uart_write_all(frame_buf, frame_len);
            clear_vsr_log_messages(vsr);
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
