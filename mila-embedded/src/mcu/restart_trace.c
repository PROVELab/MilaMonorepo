#include "restart_trace.h"

#include "esp_attr.h"
#include "esp_system.h"

#include "esp_log.h"


#include <stdint.h>
#include <inttypes.h>

typedef enum {
    MCU_RESTART_TRACE_NONE,
    MCU_RESTART_TRACE_ESP_RESTART_CALLER,
    MCU_RESTART_TRACE_UNCAPTURED_SOFTWARE_RESET,
} mcu_restart_trace_result_t;

// RTC_NOINIT_ATTR survives esp_restart(), unlike ordinary DRAM. The linker
// option in src/CMakeLists.txt redirects calls to esp_restart() here.
#define MCU_RESTART_TRACE_MAGIC UINT32_C(0x4D435254) // "MCRT"

RTC_NOINIT_ATTR static uint32_t restart_trace_magic;
RTC_NOINIT_ATTR static uintptr_t restart_trace_caller_pc;

void __real_esp_restart(void);

mcu_restart_trace_result_t mcu_restart_trace_take_previous(uintptr_t* caller_pc,
                                                            int* reset_reason);

void log_restart_trace(){
    uintptr_t restart_caller_pc = 0;
    int reset_reason = 0;
    const mcu_restart_trace_result_t restart_trace =
        mcu_restart_trace_take_previous(&restart_caller_pc, &reset_reason);
    ESP_LOGW(__func__, "boot reset reason=%d", reset_reason);
    if (restart_trace == MCU_RESTART_TRACE_ESP_RESTART_CALLER) {
        ESP_LOGE(__func__, "restart trace: esp_restart caller PC=0x%08" PRIxPTR, restart_caller_pc);
    } else if (restart_trace == MCU_RESTART_TRACE_UNCAPTURED_SOFTWARE_RESET) {
        ESP_LOGE(__func__, "restart trace: software reset without a captured esp_restart caller");
    }
}

void __wrap_esp_restart(void) {
    restart_trace_caller_pc = (uintptr_t)__builtin_return_address(0);
    restart_trace_magic = MCU_RESTART_TRACE_MAGIC;
    __real_esp_restart();
    __builtin_unreachable();
}

mcu_restart_trace_result_t mcu_restart_trace_take_previous(uintptr_t* caller_pc, int* reset_reason) {
    const esp_reset_reason_t detected_reset_reason = esp_reset_reason();
    if (reset_reason != NULL) { *reset_reason = (int) detected_reset_reason; }

    // The marker is authoritative: do not discard it merely because the reset
    // reason read by IDF differs from the ROM's rst:0xc banner.
    if (restart_trace_magic == MCU_RESTART_TRACE_MAGIC) {
        if (caller_pc != NULL) { *caller_pc = restart_trace_caller_pc; }
        restart_trace_magic = 0;
        restart_trace_caller_pc = 0;
        return MCU_RESTART_TRACE_ESP_RESTART_CALLER;
    } else if (detected_reset_reason == ESP_RST_SW) {
        return MCU_RESTART_TRACE_UNCAPTURED_SOFTWARE_RESET;
    }

    return MCU_RESTART_TRACE_NONE;
}
