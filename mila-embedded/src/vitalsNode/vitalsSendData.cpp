#include "esp_log.h"

#include "vitalsHelper/vitalsPacketSendLUT.h"
#include "../pecan/pecan.h"

const char* TAG = "vitalsSendData";

// //total packet bits includes mask bits
// void sendPacketBasic(const uint32_t mask, const size_t maskBits, const uint8_t* packet, size_t dataBytes, size_t total_packetBits){
    
// }

void sendPacket(const uint8_t* packet, size_t dataBytes){
    ESP_LOGI(TAG, "\nsending data:");
    for(size_t i=0; i<dataBytes;i++){
        ESP_LOGI(TAG, "%0x", packet[i]);
    
    }
}


int formatPacketCore(const simpleDataPoint* fields, size_t numFields, const int* data, uint8_t* tempData, size_t tempBytes){
    int8_t currBit = 0;

    for (size_t i = 0; i < numFields; i++) {
        simpleDataPoint info = fields[i];
        
        uint32_t unsignedConstrained = formatValue(data[i], info.min, info.max); 
        copyValueToData(&unsignedConstrained, tempData, currBit, info.bits);
        currBit += info.bits;
    }
    return currBit / 8;
}

uint8_t variableDataBuffer [255];
static SemaphoreHandle_t variableDataMutex = NULL; //mutex to take or add to msg queue.
static StaticSemaphore_t variableDataMutexBuffer;

void sendPacketVariable(const simpleDataPoint* fields, size_t numFields, const int* data, const uint8_t* payload, const uint8_t payloadBytes){
    int dataBufferBytes = formatPacketCore(fields, numFields, data, variableDataBuffer, dataBufferBytes);

    if (variableDataMutex == NULL){
        variableDataMutex = xSemaphoreCreateMutexStatic(&variableDataMutexBuffer);
    }
    xSemaphoreTake(variableDataMutex, portMAX_DELAY);
    memcpy(variableDataBuffer + dataBufferBytes, payload, payloadBytes);

    sendPacket(variableDataBuffer, dataBufferBytes + payloadBytes);

    xSemaphoreGive(variableDataMutex);
}


void sendPacketCore(const simpleDataPoint* fields, size_t numFields, const int* data, uint8_t* dataBuffer) {
    formatPacketCore(fields, numFields, data, dataBuffer, dataBufferBytes);

    sendPacket(dataBuffer, dataBufferBytes);

}