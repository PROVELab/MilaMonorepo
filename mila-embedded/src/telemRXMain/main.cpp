#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_vfs_dev.h"
#include <cstring>

#include "../LoraCommon/LoraProtocol.h"
#include "../LoraCommon/blastProtocolConfig.hpp"
#include "UART_COM.h" // For UART Tasks and Functions

  #include "driver/uart_vfs.h"
static const char* TAG = "main";

// --- Task Configurations ---
#define TASK_STACK_SIZE 4096

//should be plenty big
#define UART_TX_SIZE 4096
#define UART_RX_SIZE 1024

StaticTask_t LORA_Monitor_Buffer;
StackType_t LORA_Monitor_Stack[TASK_STACK_SIZE];

StaticTask_t LORA_Read_Buffer;
StackType_t LORA_Read_Stack[TASK_STACK_SIZE];

StaticTask_t UART_Rx_Buffer;
StackType_t UART_Rx_Stack[TASK_STACK_SIZE];

struct UartMetadataPayload {
    float rssi;
    float snr;
    uint32_t irqFlags;
    size_t dataSize;
    uint8_t data[255]; // Max LoRa packet size
} __attribute__((packed));

driverRecvPacket packet;

// Task: Reads from LoRa Driver and routes to UART
void loraReadTask(void* pvParameters){
    ESP_LOGI(TAG, "Lora Read Task started");
    for(;;){
        if(protocolRecv(&packet)) {
            // Use a struct for safer and more efficient payload construction.
            // This avoids using a Variable Length Array (VLA) on the stack.
            UartMetadataPayload uart_payload;
            uart_payload.rssi = packet.RSSI;
            uart_payload.snr = packet.SNR;
            uart_payload.irqFlags = packet.irqFlags;
            uart_payload.dataSize = packet.dataSize;
            memcpy(uart_payload.data, packet.data, packet.dataSize);

            size_t total_payload_size = offsetof(UartMetadataPayload, data) + packet.dataSize;
            LORA_TO_UART(reinterpret_cast<uint8_t*>(&uart_payload), total_payload_size);
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
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
        .flags = {
            .allow_pd = 0,
            .backup_before_sleep = 0,
        },
    };
    uart_param_config(UART_PORT_NUM, &uart_config);
    uart_driver_install(UART_PORT_NUM, UART_RX_SIZE, UART_TX_SIZE, 0, NULL, 0);

    //very important, otherwise binary and logging ASCII data gets jumbled, and we lose a lot of packets
      uart_vfs_dev_use_driver(UART_PORT_NUM);  // to put ESP_LOG+printf and uart_write on the same mutex for outputs


    vTaskDelay(pdMS_TO_TICKS(2000));
    ESP_LOGI(TAG, "Base Station Starting...");

    // 2. Spawn Static RTOS Tasks
    xTaskCreateStatic(loraMonitorTask, "Lora_Monitor_Task", TASK_STACK_SIZE, NULL, 1, LORA_Monitor_Stack, &LORA_Monitor_Buffer);
    xTaskCreateStatic(loraReadTask, "Lora_Read_Task", TASK_STACK_SIZE, NULL, 1, LORA_Read_Stack, &LORA_Read_Buffer);
    xTaskCreateStatic(uartRxTask, "UART_Rx_Task", TASK_STACK_SIZE, NULL, 1, UART_Rx_Stack, &UART_Rx_Buffer);
}