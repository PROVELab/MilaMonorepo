#ifndef VSR_UART_SHARED_H
#define VSR_UART_SHARED_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/uart.h"

#define VSR_UART_NUM                 UART_NUM_0
#define VSR_UART_BAUD_RATE           921600
#define VSR_UART_FRAME_MAGIC_0       0xA5u
#define VSR_UART_FRAME_MAGIC_1       0x5Au
#define VSR_UART_FRAME_HEADER_LEN    4
#define VSR_UART_RX_RING_BUFFER_LEN  1024
#define VSR_UART_TX_RING_BUFFER_LEN  1024

bool vsr_uart_init(void);
bool vsr_build_framed_packet(const uint8_t* payload, size_t payload_len, uint8_t* frame_buf, size_t frame_capacity,
                             size_t* frame_len);
void vsr_uart_write_all(const uint8_t* data, size_t len);
void vsr_append_stream_bytes(uint8_t* stream_buf, size_t* stream_len, size_t stream_capacity, const uint8_t* data,
                             size_t data_len);

#endif
