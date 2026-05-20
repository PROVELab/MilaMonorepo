#include "esp_log.h"

#include "vitalsHelper/vitalsPacketSendLUT.h"
#include "../LoraCommon/LoraProtocol.h"
#include <string.h>
#include <inttypes.h>


static const char* TAG = "vitalsSendData";

// //total packet bits includes mask bits
// void sendPacketBasic(const uint32_t mask, const size_t maskBits, const uint8_t* packet, size_t dataBytes, size_t total_packetBits){
    
// }

// Returns the number of bytes written to tempData
uint8_t formatPacketCore(const simpleDataPoint* fields, size_t numFields, const int32_t* data, uint8_t* tempData){
    int8_t currBit = 0;

    for (size_t i = 0; i < numFields; i++) {
        const simpleDataPoint* info = &fields[i];
        pecan_pack(tempData, &currBit, data[i], info);
    }
    return (currBit + 7) / 8;
}

uint8_t variableDataBuffer [255];
static SemaphoreHandle_t variableDataMutex = NULL; //mutex to take or add to msg queue.
static StaticSemaphore_t variableDataMutexBuffer;

void sendPacketVariable(const simpleDataPoint* fields, size_t numFields, const int32_t* data, const uint8_t* payload, const uint8_t payloadBytes){
    if (variableDataMutex == NULL){
        variableDataMutex = xSemaphoreCreateMutexStatic(&variableDataMutexBuffer);
    }
    xSemaphoreTake(variableDataMutex, portMAX_DELAY);

    uint8_t dataBufferBytes = formatPacketCore(fields, numFields, data, variableDataBuffer);

    memcpy(variableDataBuffer + dataBufferBytes, payload, payloadBytes);
    protocolTransmit((uint8_t*) variableDataBuffer, dataBufferBytes + payloadBytes);

    xSemaphoreGive(variableDataMutex);
}
 
void sendPacketCore(const simpleDataPoint* fields, size_t numFields, const int32_t* data, uint8_t* dataBuffer) {
    uint8_t dataBufferBytes = formatPacketCore(fields, numFields, data, dataBuffer);
    protocolTransmit(dataBuffer, dataBufferBytes);   
}