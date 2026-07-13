#include <cstring>
#include <stdio.h>     // For snprintf

#include "esp_timer.h" // For esp_timer_get_time()
#include "esp_log.h"

#include "UART_COM.h"



// Define a max frame size to avoid VLAs and stack overflows.
// Max LoRa packet is 255, plus 16 for metadata, plus 5 for UART framing.
#define MAX_UART_FRAME_SIZE 300

void LORA_TO_UART(const uint8_t* data, size_t dataSize) {
    // --- Build the entire packet in one buffer to ensure atomicity of the write ---

    // 2. Compute 16-bit checksum over the actual LoRa payload (data).
    uint16_t csum = in_cksum(data, dataSize);

    // 3. Assemble the full packet into a single buffer for an atomic write.
    // Format: [SOF][LEN_LO][LEN_HI][PAYLOAD...][CSUM_LO][CSUM_HI]
    const size_t binary_header_len = 1 + 2; // SOF + Length
    const size_t binary_footer_len = 2;     // Checksum
    size_t total_packet_size = binary_header_len + dataSize + binary_footer_len;

    if (total_packet_size > MAX_UART_FRAME_SIZE) {
        // This should not happen with the current UartMetadataPayload struct
        ESP_LOGE("UART_send", "Payload too large for UART frame: %d > %d", total_packet_size, MAX_UART_FRAME_SIZE);
        return;
    }
    // Use a static buffer to avoid using a VLA on the stack.
    static uint8_t full_packet[MAX_UART_FRAME_SIZE];
    uint8_t* ptr = full_packet;

    // Add SOF
    *ptr++ = SOF;

    // Add Payload Length (Little Endian)
    *ptr++ = (uint8_t)(dataSize & 0xFF);
    *ptr++ = (uint8_t)((dataSize >> 8) & 0xFF);

    // Add Payload (the LoRa data)
    memcpy(ptr, data, dataSize);
    ptr += dataSize;

    // Add Checksum (Little Endian)
    *ptr++ = (uint8_t)(csum & 0xFF);
    *ptr++ = (uint8_t)((csum >> 8) & 0xFF);

    // 4. Send the whole thing in one go to prevent task-switching corruption.
    uart_write_bytes(UART_PORT_NUM, full_packet, total_packet_size);
}
