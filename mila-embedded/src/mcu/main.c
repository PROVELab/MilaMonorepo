#include "driver/gpio.h"
#include "driver/twai.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../espBase/debug_esp.h"
#include "../pecan/pecan.h" //helper code for CAN stuff

#include "esp_log.h"
#include "motor_h300/h300.h"
#include "tasks/tasks.h"
#include "tasks/vsr_uart_shared.h"
#include "vsr/vsr_state.h" // vehicle status register, holds all the information about the vehicle

#include "sensors/pedalSensor/pedal_sensor.h" // for pedal reading

// === CAN static mem === //

void setup_motor_controller_params(PCANListenParamsCollection* plpc) {
    CANListenParam process_motor_fxn_code1 = {
        .listen_id = combinedID(1, 0),
        .handler = parse_packet_motor,
        .mt = MATCH_FUNCTION,
    };

    CANListenParam process_motor_fxn_code2 = {
        .listen_id = combinedID(0, 0),
        .handler = parse_packet_motor,
        .mt = MATCH_FUNCTION,
    };

    addParam(plpc, process_motor_fxn_code1);
    addParam(plpc, process_motor_fxn_code2);
}

static void vsr_log_append_line(vehicle_status_reg_t* vsr, const char* line) {
    const size_t max_messages = sizeof(VSR_DATA.log.message) / sizeof(VSR_DATA.log.message[0]);
    const size_t max_len = sizeof(VSR_DATA.log.message[0]);
    if (max_messages == 0 || max_len < 2) { return; }

    if ((size_t) VSR_DATA.log.message_count >= max_messages) { return; }

    const size_t idx = (size_t) VSR_DATA.log.message_count;
    strncpy(VSR_DATA.log.message[idx], line, max_len - 1);
    VSR_DATA.log.message[idx][max_len - 1] = '\0';
    VSR_DATA.log.message_count = (pb_size_t) (idx + 1);
}

static int vsr_log_vprintf(const char* fmt, va_list ap) {
    vehicle_status_reg_t* vsr = &vsr_global;
    char buf[128];
    int len = vsnprintf(buf, sizeof(buf), fmt, ap);
    if (len < 0) { return len; }

    size_t used = strnlen(buf, sizeof(buf));
    while (used > 0 && (buf[used - 1] == '\n' || buf[used - 1] == '\r')) {
        buf[used - 1] = '\0';
        used--;
    }
    if (used == 0) { return len; }

    if (vsr->log_mutex != NULL) {
        ACQ_REL_VSRSEM_W(vsr, log, { vsr_log_append_line(vsr, buf); });
    }

    return len;
}

void app_main() {
    // Stream-only mode for USB serial: no text logging on UART0.
    setMutexPrintEnabled(false);
    esp_log_level_set("*", ESP_LOG_INFO);

    // Initialize the global VSR
    vsr_init(&vsr_global);
    esp_log_set_vprintf(&vsr_log_vprintf);

    ESP_LOGI(__func__, "Hello, minimal app starting");

    if (!vsr_uart_init()) {
        ESP_LOGE(__func__, "Failed to initialize shared UART for VSR stream/command RX");
        return;
    }

    static PCANListenParamsCollection plpc = {
        .arr = {{0}},
        .defaultHandler = defaultPacketRecv,
        .size = 0,
    };

    setup_motor_controller_params(&plpc); // setup motor controller CAN handlers
    pedal_main(&plpc); // start the pedal sensor reading task, which will also add its CAN handlers and start PECAN

    ESP_LOGI(__func__, "Initialized VSR, finished TWAI config");

    // Core 0 tasks
    // start_console_task(); // TODO: implement this
    ESP_LOGI(__func__, "StartedConsole");

    // start_logging_task(); //<- replaced with SD card version.
    ESP_LOGI(__func__, "StartedLogging");

    // Send data to the motor task
    start_send_motor_task();
    start_vsr_stream_task();
    start_mcu_health_task();
    start_motor_command_rx_task();
    ESP_LOGI(__func__, "Started motor tasks");
}
