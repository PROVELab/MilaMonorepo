#include "esp_log.h"

#include "freertos/FreeRTOS.h" // For SemaphoreHandle_t, etc.
#include "../vitalsGen/vitalsPacketSendLUT.h"
#include "../../LoraCommon/LoraProtocol.h"
#include "../../pecan/pecan.h"
#include <string.h>
#include <inttypes.h>

static const char* TAG = "vitalsSendData";

//this one
void packDataStream(const int32_t* restrict values, const int numValues, uint8_t* restrict target, const simpleDataPoint* restrict dataPoints, int32_t* currBit) {
    
    for (int i = 0; i < numValues; i++) {
        //squeeze to min and max, then subtracts min to send as an unsigned value
        const uint32_t val = formatValue(*values++, dataPoints->min, dataPoints->max);
        const int bits = dataPoints->bits;
        
        //determine position within target
        uint8_t* currentTarget = target + (*currBit >> 3);
        const int bitOffset = *currBit & 7; 
        
        // write the first byte
        *currentTarget |= (uint8_t)(val << bitOffset);
        
        // shift value to align with target.
        const int topBitCount = 8 - bitOffset;
        uint32_t shiftedValue = val >> topBitCount; 
        
        // write the remaining bytes
        for (int overflowBits = bits; overflowBits > topBitCount; overflowBits -= 8) {
            *(++currentTarget) = (uint8_t)shiftedValue;
            shiftedValue >>= 8;
        }

        //advance position
        *currBit += bits;
        dataPoints++;
    }
}

void unpackDataStream(int32_t* restrict target, const int numValues, const uint8_t* restrict data, const simpleDataPoint* restrict dataPoints, int32_t* totalBits) {
    
    for (int i = 0; i < numValues; i++) {
        const int bits = dataPoints->bits;
        
        // 1. Absolute Memory Positioning
        const uint8_t* currentData = data + (*totalBits >> 3);
        const int bitOffset = *totalBits & 7;
        
        // 2. Read base bits (shifted down so they start at bit 0 of rawValue)
        uint32_t rawValue = *currentData >> bitOffset;
        
        // 3. Accumulate overflow bytes
        // We start shifting the new bytes up by exactly the number of bits we already read
        const int bitsRead = 8 - bitOffset;
        for (int shift = bitsRead; shift < bits; shift += 8) {
            rawValue |= ((uint32_t)*(++currentData)) << shift;
        }
        
        // 4. Mask off upper garbage bits
        // The ternary operator safely prevents Undefined Behavior if bits == 32 (1U << 32 is UB)
        const uint32_t mask = (bits == 32) ? 0xFFFFFFFF : ((1U << bits) - 1);
        rawValue &= mask;
        
        // 5. Restore original un-squeezed value and write to target
        *target++ = (int32_t)rawValue + dataPoints->min;
        
        // 6. Advance global state
        *totalBits += bits;
        dataPoints++;
    }
}


// Helper to pack the fixed-size header part of a packet.
// Returns the number of bytes written to tempData.
uint8_t formatPacketCore(const simpleDataPoint* fields, size_t numFields, const int32_t* data, uint8_t* tempData){
    uint16_t total_bits = 0;
    for (size_t i = 0; i < numFields; i++) {
        total_bits += fields[i].bits;
    }

    const uint8_t total_bytes = (uint8_t)((total_bits + 7u) / 8u);
    memset(tempData, 0, total_bytes);

    int32_t currBit = 0;

    packDataStream(data, numFields, tempData, fields, &currBit);

    return total_bytes;
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
    ESP_LOGI(TAG, "sendPacketCore formatted %u bytes:", (unsigned)dataBufferBytes);
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, dataBuffer, dataBufferBytes, ESP_LOG_INFO);
    protocolTransmit(dataBuffer, dataBufferBytes);   
}
