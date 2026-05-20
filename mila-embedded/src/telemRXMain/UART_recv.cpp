#include "UART_COM.h"
#include "esp_log.h"
#include <cstring>

// Include your protocol header to call protocolTransmit()
#include "../LoraCommon/LoraProtocol.h" 

static const char* TAG = "UART_Rx";

void uartRxTask(void* pvParameters) {
    ESP_LOGI(TAG, "UART Rx Task started");
    
    uint8_t frame[10] = {0}; // [CHK16 (2 bytes)][DATA (8 bytes)]
    int8_t have = -1;
    uint8_t rx_byte;

    for(;;) {
        // Block until a byte is received
        if (uart_read_bytes(UART_PORT_NUM, &rx_byte, 1, portMAX_DELAY) > 0) {
            
            if (have == -1) {
                if (rx_byte == SOF) { have = 0; }
                continue;
            }

            frame[have++] = rx_byte;

            if (have != sizeof(frame)) {
                continue; // Not enough bytes yet
            }

            // We have a full 10-byte message (after SOF)
            uint16_t chk = ((uint16_t) frame[1] << 8) | frame[0];
            const uint8_t* payload = &frame[2]; // 8 bytes of actual data

            uint16_t calc = in_cksum(payload, 8);

            if (chk == calc) {
                // Valid Message! Forward to LoRa.
                have = -1; 
                
                // --- FORWARD RAW BYTES HERE ---
                // Depending on your implementation, this might be:
                // protocolTransmit(payload, 8);
                
                continue; 
            }
            
            // Bad checksum: resync by scanning inside the 10 bytes after SOF
            int nextSOF = -1;
            for (uint8_t i = 0; i < sizeof(frame); ++i) {
                if (frame[i] == SOF) {
                    nextSOF = i;
                    break;
                }
            }
            if (nextSOF >= 0) {
                uint8_t keep = sizeof(frame) - (nextSOF + 1); 
                memmove(frame, frame + nextSOF + 1, keep);    
                have = keep;                                  
            } else {
                have = -1; 
            }
        }
    }
}