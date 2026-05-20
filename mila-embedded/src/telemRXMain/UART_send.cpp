#include "UART_COM.h"
#include <cstring>
#include "esp_timer.h" // For esp_timer_get_time()
#include <stdio.h>     // For snprintf


void LORA_TO_UART(const uint8_t* data, size_t dataSize) {
    // Prepend a 'B' and timestamp to the start of the packet for identification.
    // This is sent immediately before the binary SOF.
    char prefix_buffer[64];
    int prefix_len = snprintf(prefix_buffer, sizeof(prefix_buffer), "B (%llu) ", esp_timer_get_time() / 1000);
    uart_write_bytes(UART_PORT_NUM, (const uint8_t*)prefix_buffer, prefix_len);

    // Build 12-byte payload (4 byte ID + 8 byte DATA structure)
    uint8_t payload[12] = {0};
    
    size_t copySize = (dataSize > sizeof(payload)) ? sizeof(payload) : dataSize;
    std::memcpy(payload, data, copySize);

    // Compute 16-bit checksum over payload bytes
    uint16_t csum = in_cksum(payload, sizeof(payload));

    // Construct the 3-byte header
    uint8_t header[3];
    header[0] = SOF;
    header[1] = (uint8_t)(csum & 0xFF); // low byte
    header[2] = (uint8_t)(csum >> 8);   // high byte

    // Send Header + Payload
    uart_write_bytes(UART_PORT_NUM, header, sizeof(header));
    uart_write_bytes(UART_PORT_NUM, payload, sizeof(payload));
}

// void sendUARTErrorFlag(int8_t flag) {
//     uint8_t errorPayload[12] = {0};
//     // Format error flag (adjust index based on your specific ID/Data mapping)
//     errorPayload[4] = flag; 
    
//     LORA_TO_UART(errorPayload, sizeof(errorPayload));
// }