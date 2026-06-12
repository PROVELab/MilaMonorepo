#include "esp_log.h"

#include "freertos/FreeRTOS.h" // For SemaphoreHandle_t, etc.
#include "vitalsGen/vitalsPacketSendLUT.h"
#include "../LoraCommon/LoraProtocol.h"
#include "../pecan/pecan.h"
#include <string.h>
#include <inttypes.h>

static const char* TAG = "vitalsSendData";

// Helper to pack the fixed-size header part of a packet.
// Returns the number of bytes written to tempData.
uint8_t formatPacketCore(const simpleDataPoint* fields, size_t numFields, const int32_t* data, uint8_t* tempData){
    int8_t currBit = 0;

    for (size_t i = 0; i < numFields; i++) {
        const simpleDataPoint* info = &fields[i];
        pecan_pack(tempData, &currBit, data[i], info);
    }
    return (currBit + 7) / 8;
}

uint8_t variableDataBuffer [maxLoraPacketSize - 4]; //size of largest acceptable packet
static SemaphoreHandle_t variableDataMutex = NULL;
static StaticSemaphore_t variableDataMutexBuffer;

//returns number of bytes to transmit.
//returns 0 on error (so shouldnt transmit any bytes)
uint8_t formatPacketVariable(const simpleDataPoint* fields, size_t numFields, const int32_t* data, const uint8_t* payload, size_t payloadBytes, uint8_t* outBuffer){
    // Format the header into the buffer first
    const uint8_t headerBytes = formatPacketCore(fields, numFields, data, outBuffer);
    const uint8_t totalBytes = headerBytes + payloadBytes;
    if(totalBytes > sizeof(variableDataBuffer)){
        ESP_LOGW(TAG, "Attempted to format packet thats too big. returning 0 size! attempted size: %" PRIu8, totalBytes);
        return 0;
    }
    // Append the payload directly after the header
    memcpy(outBuffer + headerBytes, payload, payloadBytes);
    return totalBytes;
}

void sendPacketVariable(const simpleDataPoint* fields, size_t numFields, const int32_t* data, const uint8_t* payload, size_t payloadBytes){
    //mutex cuz re-uses the same buffer across all calls.
    if (variableDataMutex == NULL){
        variableDataMutex = xSemaphoreCreateMutexStatic(&variableDataMutexBuffer);
    }
    
    if (xSemaphoreTake(variableDataMutex, portMAX_DELAY) == pdTRUE) {
        const uint8_t dataBufferBytes = formatPacketVariable(fields, numFields, data, payload, payloadBytes, variableDataBuffer);
        protocolTransmit((uint8_t*) variableDataBuffer, dataBufferBytes);
        xSemaphoreGive(variableDataMutex);
    }
}

void sendPacketCore(const simpleDataPoint* fields, size_t numFields, const int32_t* data, uint8_t* dataBuffer) {
    const uint8_t dataBufferBytes = formatPacketCore(fields, numFields, data, dataBuffer);
    protocolTransmit(dataBuffer, dataBufferBytes);   
}
