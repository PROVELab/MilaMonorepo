#include "tasks.h"

#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdint.h>
#include <string.h>

#define MCU_HEALTH_PERIOD_MS      1000
#define MCU_HEALTH_MAX_TASK_STATS 32U

static float sample_cpu_load_pct(void) {
    TaskStatus_t stats[MCU_HEALTH_MAX_TASK_STATS];
    uint32_t total_runtime = 0;
    UBaseType_t count = uxTaskGetSystemState(stats, MCU_HEALTH_MAX_TASK_STATS, &total_runtime);
    if (count == 0 || total_runtime == 0) { return 0.0f; }

    uint32_t idle_runtime = 0;
    for (UBaseType_t i = 0; i < count; ++i) {
        if (strncmp(stats[i].pcTaskName, "IDLE", 4) == 0) { idle_runtime += stats[i].ulRunTimeCounter; }
    }

    float idle_pct = ((float) idle_runtime * 100.0f) / (float) total_runtime;
    if (idle_pct > 100.0f) { idle_pct = 100.0f; }
    return 100.0f - idle_pct;
}

static void mcu_health_main(void* arg) {
    (void) arg;

    volatile vehicle_status_reg_t* vsr = &vsr_global;
    const uint32_t heap_total_bytes = heap_caps_get_total_size(MALLOC_CAP_8BIT);
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(MCU_HEALTH_PERIOD_MS);

    while (1) {
        const uint32_t free_heap_bytes = esp_get_free_heap_size();
        const uint32_t used_heap_bytes = (heap_total_bytes > free_heap_bytes) ? (heap_total_bytes - free_heap_bytes) : 0;
        const float heap_used_pct =
            (heap_total_bytes > 0) ? (((float) used_heap_bytes * 100.0f) / (float) heap_total_bytes) : 0.0f;

        ACQ_REL_VSRSEM_W(vsr, mcu_health, {
            VSR_DATA.mcu_health.uptime_s = (uint32_t) (esp_timer_get_time() / 1000000ULL);
            VSR_DATA.mcu_health.cpu_load_pct = sample_cpu_load_pct();
            VSR_DATA.mcu_health.free_heap_bytes = free_heap_bytes;
            VSR_DATA.mcu_health.min_free_heap_bytes = esp_get_minimum_free_heap_size();
            VSR_DATA.mcu_health.heap_used_pct = heap_used_pct;
            VSR_DATA.mcu_health.task_count = uxTaskGetNumberOfTasks();
        });

        xTaskDelayUntil(&last_wake_time, period);
    }
}

void start_mcu_health_task() {
    static StackType_t mcu_health_stack[DEFAULT_STACK_SIZE];
    static StaticTask_t mcu_health_tcb;

    xTaskCreateStaticPinnedToCore(mcu_health_main, "mcu_health", DEFAULT_STACK_SIZE, NULL, MCU_HEALTH_TASK_PRIO,
                                  mcu_health_stack, &mcu_health_tcb, 0);
}
