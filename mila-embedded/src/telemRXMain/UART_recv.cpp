#include "UART_COM.h"
#include "esp_log.h"
#include <cstring>
#include <vector>

// Include your protocol header to call protocolTransmit()
#include "../LoraCommon/LoraProtocol.h" 

static const char* TAG = "UART_Rx";

#define RX_BUFFER_SIZE 256
// Use a buffer twice the size to handle junk data before a valid frame
#define STATIC_RX_BUFFER_SIZE (RX_BUFFER_SIZE * 2)

void uartRxTask(void* pvParameters) {
    ESP_LOGI(TAG, "UART Rx Task started");
    
    // Use a static buffer to avoid dynamic memory allocation
    static uint8_t rx_buffer[STATIC_RX_BUFFER_SIZE];
    static size_t buffer_len = 0;
    uint8_t temp_buf[RX_BUFFER_SIZE];

    for(;;) {
        // Read available bytes from UART
        int length = 0;
        uart_get_buffered_data_len(UART_PORT_NUM, (size_t*)&length);

        if (length > 0) {
            // Calculate how much we can read without overflowing our static buffer
            size_t space_available = sizeof(rx_buffer) - buffer_len;
            size_t read_len = (length > space_available) ? space_available : (size_t)length;
            read_len = (read_len > sizeof(temp_buf)) ? sizeof(temp_buf) : read_len;

            if (read_len > 0) {
                int bytes_read = uart_read_bytes(UART_PORT_NUM, temp_buf, read_len, 20 / portTICK_PERIOD_MS);
                if (bytes_read > 0) {
                    memcpy(rx_buffer + buffer_len, temp_buf, bytes_read);
                    buffer_len += bytes_read;
                }
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(10)); // Don't spin too fast
            continue;
        }

        // Process all complete frames in the buffer
        while (buffer_len > 0) {
            // 1. Find Start of Frame
            uint8_t* sof_ptr = (uint8_t*)memchr(rx_buffer, SOF, buffer_len);
            if (sof_ptr == NULL) {
                ESP_LOGW(TAG, "No SOF in buffer, clearing %d bytes of junk.", (int)buffer_len);
                buffer_len = 0;
                break; // Wait for more data
            }

            // Discard junk before SOF
            size_t junk_len = sof_ptr - rx_buffer;
            if (junk_len > 0) {
                ESP_LOGW(TAG, "Discarding %d bytes of junk before SOF", (int)junk_len);
                memmove(rx_buffer, rx_buffer + junk_len, buffer_len - junk_len);
                buffer_len -= junk_len;
            }

            // 2. Check for minimum frame length: SOF(1) + LEN(2) + CSUM(2)
            constexpr size_t MIN_FRAME_LEN = 1 + 2 + 2;
            if (buffer_len < MIN_FRAME_LEN) {
                break; // Not enough data for a minimal frame yet
            }

            // 3. Get payload length (Little Endian)
            size_t payload_len = rx_buffer[1] | (rx_buffer[2] << 8);
            
            if (payload_len > RX_BUFFER_SIZE) {
                ESP_LOGE(TAG, "Invalid payload length: %d. Discarding SOF.", (int)payload_len);
                memmove(rx_buffer, rx_buffer + 1, buffer_len - 1); // Discard bad SOF
                buffer_len--;
                continue; // Try to re-sync
            }

            size_t total_frame_len = 1 + 2 + payload_len + 2;

            if (buffer_len < total_frame_len) {
                break; // Not enough data yet
            }

            uint8_t* payload_ptr = &rx_buffer[3];
            uint16_t received_csum = rx_buffer[3 + payload_len] | (rx_buffer[3 + payload_len + 1] << 8);
            uint16_t calculated_csum = in_cksum(payload_ptr, payload_len);

            if (received_csum != calculated_csum) {
                ESP_LOGW(TAG, "Checksum mismatch! Got 0x%04X, expected 0x%04X. Discarding SOF.", received_csum, calculated_csum);
                memmove(rx_buffer, rx_buffer + 1, buffer_len - 1); // Discard bad SOF
                buffer_len--;
                continue;
            }

            ESP_LOGI(TAG, "Received valid command frame with payload size %d", (int)payload_len);
            protocolTransmit(payload_ptr, payload_len);

            // Remove the processed frame from the buffer by shifting the remaining data
            memmove(rx_buffer, rx_buffer + total_frame_len, buffer_len - total_frame_len);
            buffer_len -= total_frame_len;
        }
    }
}