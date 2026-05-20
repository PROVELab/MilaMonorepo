#pragma once

#include <stdint.h>
#include <stddef.h>
#include "driver/uart.h"

#define SOF 0xFF // Start Of Frame byte
#define UART_PORT_NUM UART_NUM_0 // Default ESP32 USB console port

// Checksum
uint16_t in_cksum(const uint8_t* addr, int len);

// UART Send
void LORA_TO_UART(const uint8_t* data, size_t dataSize);
void sendUARTErrorFlag(int8_t flag);

// UART Recv Task
void uartRxTask(void* pvParameters);