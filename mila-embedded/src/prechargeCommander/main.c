
#include "driver/uart.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "../pecan/pecan.h"
#include "../programConstants.h"

static const char* TAG = "prechargeCommander";

static void send_precharge_command(int8_t command) {
    CANPacket packet = {0};
    packet.id = combinedID(vitalsCommand, prechargeID);

    if (writeData(&packet, &command, 1) != SUCCESS) {
        ESP_LOGE(TAG, "failed to create precharge command");
        return;
    }

    sendPacket(&packet);
    ESP_LOGI(TAG, "sent %s command to precharge", command == enableContactor ? "enable/precharge" : "disable");
}

static void command_task(void* parameter) {
    (void) parameter;

    for (;;) {
        uint8_t input;
        if (uart_read_bytes(UART_NUM_0, &input, 1, portMAX_DELAY) != 1) continue;

        switch (input) {
            case 'p':
            case 'P':
            case 'e':
            case 'E':
                // The precharge node enters Precharging after receiving enableContactor.
                send_precharge_command(enableContactor);
                break;
            case 'd':
            case 'D': send_precharge_command(disableContactor); break;
            case '\r':
            case '\n': break;
            default: ESP_LOGW(TAG, "unknown command '%c'; use p/e to precharge or d to disable", input); break;
        }
    }
}

void app_main(void) {
    const esp_err_t uart_err = uart_driver_install(UART_NUM_0, 256, 0, 0, NULL, 0);
    if (uart_err != ESP_OK && uart_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "failed to install UART0 input driver: %s", esp_err_to_name(uart_err));
        return;
    }

    pecanInit config = {.nodeId = telemetryID, .pin1 = defaultPin, .pin2 = defaultPin};
    pecan_CanInit(config);

    ESP_LOGI(TAG, "type p or e to enable/precharge; type d to disable");
    xTaskCreate(command_task, "prechargeCommand", 4096, NULL, 5, NULL);
}
