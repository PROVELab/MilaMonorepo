#include "UART_COM.h"
#include <cstring>
#include "esp_timer.h" // For esp_timer_get_time()
#include <stdio.h>     // For snprintf


void LORA_TO_UART(const uint8_t* data, size_t dataSize) {
    // --- Build the entire packet in one buffer to ensure atomicity of the write ---

    // 1. Create the text prefix.
    char prefix_buffer[64];
    int prefix_len = snprintf(prefix_buffer, sizeof(prefix_buffer), "B (%llu) ", esp_timer_get_time() / 1000);

    // 2. Compute 16-bit checksum over the actual LoRa payload (data).
    uint16_t csum = in_cksum(data, dataSize);

    // 3. Assemble the full packet into a single buffer for an atomic write.
    // Format: [B-prefix][SOF][LEN_LO][LEN_HI][PAYLOAD...][CSUM_LO][CSUM_HI]
    const size_t binary_header_len = 1 + 2; // SOF + Length
    const size_t binary_footer_len = 2;     // Checksum
    size_t total_packet_size = prefix_len + binary_header_len + dataSize + binary_footer_len;
    
    // Use a Variable Length Array (VLA), which is supported by the ESP-IDF compiler.
    uint8_t full_packet[total_packet_size];
    uint8_t* ptr = full_packet;

    // Copy prefix
    memcpy(ptr, prefix_buffer, prefix_len);
    ptr += prefix_len;

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

// void sendUARTErrorFlag(int8_t flag) {
//     uint8_t errorPayload[12] = {0};
//     // Format error flag (adjust index based on your specific ID/Data mapping)
//     errorPayload[4] = flag; 
    
//     LORA_TO_UART(errorPayload, sizeof(errorPayload));
// }