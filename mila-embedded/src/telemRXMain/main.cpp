#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"

#include "../LoraCommon/LoraProtocol.h"
#include "UART_COM.h" // For UART Tasks and Functions

static const char* TAG = "main";

// --- Task Configurations ---
#define TASK_STACK_SIZE 4096

StaticTask_t LORA_Monitor_Buffer;
StackType_t LORA_Monitor_Stack[TASK_STACK_SIZE];

StaticTask_t LORA_Read_Buffer;
StackType_t LORA_Read_Stack[TASK_STACK_SIZE];

StaticTask_t UART_Rx_Buffer;
StackType_t UART_Rx_Stack[TASK_STACK_SIZE];

driverRecvPacket packet;

// Task: Reads from LoRa Driver and routes to UART
void loraReadTask(void* pvParameters){
    ESP_LOGI(TAG, "Lora Read Task started");
    for(;;){
        if(protocolRecv(&packet)) {
            LORA_TO_UART(packet.data, packet.dataSize);
        } else {
            ESP_LOGW(TAG, "No packets received (Timeout)");
        }
    }
}

// Task: Monitors LoRa Driver for crashes
void loraMonitorTask(void* pvParameters){
    ESP_LOGI(TAG, "Lora Monitor Task started");
    RadioConfig cfg = getStandardConfig(BoardType::Ebyte_SX1262, TestMode::lowPower);
    for(;;){
        char* errMsg = NULL;
        int16_t errCode = runProtocol(&cfg, &errMsg);
        ESP_LOGE(TAG, "Lora Driver crashed with code %d, msg: %s", errCode, errMsg);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// Application Entry Point
extern "C" void app_main(void) {
    // 1. Initialize UART Hardware
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_param_config(UART_PORT_NUM, &uart_config);
    uart_driver_install(UART_PORT_NUM, 1024, 1024, 0, NULL, 0);

    vTaskDelay(pdMS_TO_TICKS(2000));
    ESP_LOGI(TAG, "Base Station Starting...");

    // 2. Spawn Static RTOS Tasks
    xTaskCreateStatic(loraMonitorTask, "Lora_Monitor_Task", TASK_STACK_SIZE, NULL, 1, LORA_Monitor_Stack, &LORA_Monitor_Buffer);
    xTaskCreateStatic(loraReadTask, "Lora_Read_Task", TASK_STACK_SIZE, NULL, 1, LORA_Read_Stack, &LORA_Read_Buffer);
    xTaskCreateStatic(uartRxTask, "UART_Rx_Task", TASK_STACK_SIZE, NULL, 1, UART_Rx_Stack, &UART_Rx_Buffer);
}