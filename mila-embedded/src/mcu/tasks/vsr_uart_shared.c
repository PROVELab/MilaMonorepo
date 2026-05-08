#include "vsr_uart_shared.h"

#include "esp_err.h"

#include <string.h>

bool vsr_uart_init(void) {
    const uart_config_t uart_config = {
        .baud_rate = VSR_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    // UART0 may already have a driver installed by IDF startup code.
    esp_err_t err =
        uart_driver_install(VSR_UART_NUM, VSR_UART_RX_RING_BUFFER_LEN, VSR_UART_TX_RING_BUFFER_LEN, 0, NULL, 0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) { return false; }

    if (uart_param_config(VSR_UART_NUM, &uart_config) != ESP_OK) { return false; }

    if (uart_set_pin(VSR_UART_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) !=
        ESP_OK) {
        return false;
    }

    return true;
}

bool vsr_build_framed_packet(const uint8_t* payload, size_t payload_len, uint8_t* frame_buf, size_t frame_capacity,
                             size_t* frame_len) {
    if (payload == NULL || frame_buf == NULL || frame_len == NULL) { return false; }
    if (payload_len == 0 || payload_len > UINT16_MAX) { return false; }

    size_t encoded_len = VSR_UART_FRAME_HEADER_LEN + payload_len;
    if (encoded_len > frame_capacity) { return false; }

    frame_buf[0] = VSR_UART_FRAME_MAGIC_0;
    frame_buf[1] = VSR_UART_FRAME_MAGIC_1;
    frame_buf[2] = (uint8_t) (payload_len & 0xFFu);
    frame_buf[3] = (uint8_t) ((payload_len >> 8) & 0xFFu);
    memcpy(&frame_buf[VSR_UART_FRAME_HEADER_LEN], payload, payload_len);

    *frame_len = encoded_len;
    return true;
}

void vsr_uart_write_all(const uint8_t* data, size_t len) {
    size_t offset = 0;
    while (offset < len) {
        int written = uart_write_bytes(VSR_UART_NUM, (const char*) data + offset, len - offset);
        if (written <= 0) { break; }
        offset += (size_t) written;
    }
}

void vsr_append_stream_bytes(uint8_t* stream_buf, size_t* stream_len, size_t stream_capacity, const uint8_t* data,
                             size_t data_len) {
    if (stream_buf == NULL || stream_len == NULL || data == NULL || stream_capacity == 0 || data_len == 0) { return; }

    if (data_len >= stream_capacity) {
        memcpy(stream_buf, data + (data_len - stream_capacity), stream_capacity);
        *stream_len = stream_capacity;
        return;
    }

    if (*stream_len + data_len > stream_capacity) {
        size_t bytes_to_drop = (*stream_len + data_len) - stream_capacity;
        memmove(stream_buf, stream_buf + bytes_to_drop, *stream_len - bytes_to_drop);
        *stream_len -= bytes_to_drop;
    }

    memcpy(stream_buf + *stream_len, data, data_len);
    *stream_len += data_len;
}
