#include "esp_log.h"

#include "freertos/FreeRTOS.h" // For SemaphoreHandle_t, etc.
#include "vitalsHelper/vitalsPacketSendLUT.h"
#include "../LoraCommon/LoraProtocol.h"
#include <string.h>
#include <inttypes.h>

static const char* TAG = "vitalsSendData";

// These functions may be called from C code
extern "C" {

    // Helper to pack the fixed-size header part of a packet.
    // Returns the number of bytes written to tempData.
    static uint8_t formatPacketCore(const simpleDataPoint* fields, size_t numFields, const int32_t* data, uint8_t* tempData){
        int8_t currBit = 0;

        for (size_t i = 0; i < numFields; i++) {
            const simpleDataPoint* info = &fields[i];
            pecan_pack(tempData, &currBit, data[i], info);
        }
        return (currBit + 7) / 8;
    }

    // This buffer is shared for all variable-length packets. The mutex ensures only one task can use it at a time.
    // Ensure 255 is large enough for your biggest possible packet (header + payload).
    uint8_t variableDataBuffer [255];
    static SemaphoreHandle_t variableDataMutex = NULL;
    static StaticSemaphore_t variableDataMutexBuffer;

    void sendPacketVariable(const simpleDataPoint* fields, size_t numFields, const int32_t* data, const uint8_t* payload, size_t payloadBytes){
        if (variableDataMutex == NULL){
            variableDataMutex = xSemaphoreCreateMutexStatic(&variableDataMutexBuffer);
        }
        if (xSemaphoreTake(variableDataMutex, portMAX_DELAY) == pdTRUE) {
            uint8_t dataBufferBytes = formatPacketCore(fields, numFields, data, variableDataBuffer);

            if (dataBufferBytes + payloadBytes > sizeof(variableDataBuffer)) {
                ESP_LOGE(TAG, "Packet too large for variableDataBuffer! Size: %d", dataBufferBytes + payloadBytes);
            } else {
                memcpy(variableDataBuffer + dataBufferBytes, payload, payloadBytes);
                protocolTransmit((uint8_t*) variableDataBuffer, dataBufferBytes + payloadBytes);
            }
            xSemaphoreGive(variableDataMutex);
        }
    }
    
    void sendPacketCore(const simpleDataPoint* fields, size_t numFields, const int32_t* data, uint8_t* dataBuffer) {
        uint8_t dataBufferBytes = formatPacketCore(fields, numFields, data, dataBuffer);
        protocolTransmit(dataBuffer, dataBufferBytes);   
    }

} // extern "C"
