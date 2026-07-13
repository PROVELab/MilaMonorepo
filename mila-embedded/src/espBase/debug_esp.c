#include "debug_esp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdbool.h>
#include <stdio.h>
#include "esp_log.h"

static TaskHandle_t heapWarnTask;
static const char* TAG = "DebugESP";

#define WARN_ALLOC (1u << 0)
#define WARN_FREE  (1u << 1)

// detect allocs and hooks
void esp_heap_trace_alloc_hook(void* ptr, size_t size, uint32_t caps) {
    // Only signal if scheduler is running and task exists
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING && heapWarnTask != NULL) {
        if (xPortInIsrContext()) {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xTaskNotifyFromISR(heapWarnTask, WARN_ALLOC, eSetBits, &xHigherPriorityTaskWoken);
        } else {
            (void) xTaskNotify(heapWarnTask, WARN_ALLOC, eSetBits);
        }
    }
}

void esp_heap_trace_free_hook(void* ptr) {
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING && heapWarnTask != NULL) {
        if (xPortInIsrContext()) {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xTaskNotifyFromISR(heapWarnTask, WARN_FREE, eSetBits, &xHigherPriorityTaskWoken);
        } else {
            (void) xTaskNotify(heapWarnTask, WARN_FREE, eSetBits);
        }
    }
}

static void warning_on_alloc_task(void* arg) {
    uint32_t flags = 0;
    for (;;) {
        // Wait for any bits to be set; clear all bits on exit
        xTaskNotifyWait(0, UINT32_MAX, &flags, portMAX_DELAY);
        if (flags & WARN_ALLOC) { ESP_LOGW(TAG, "Allocating memory!"); }
        if (flags & WARN_FREE) { ESP_LOGW(TAG, "Freeing memory!"); }
    }
}

void base_ESP_init(void) {
    // Small stack is fine; it only prints.
    (void) xTaskCreate(warning_on_alloc_task, "heap_warn", 1024, NULL, 1, &heapWarnTask);
}
