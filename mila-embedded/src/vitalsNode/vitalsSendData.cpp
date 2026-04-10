#include "esp_log.h"

#include "vitalsHelper/vitalsPacketSendLUT.h"
#include "../pecan/pecan.h"

const char* TAG = "vitalsSendData";

//total packet bits includes mask bits
void sendPacketBasic(const uint32_t mask, const size_t maskBits, const uint8_t* packet, size_t dataBytes, size_t total_packetBits){
    
}
void sendPacketCore(const simpleDataPoint* fields, size_t numFields, const int* data, uint8_t* tempData, size_t tempBytes) {
    int8_t currBit = 0;

    for (size_t i = 0; i < numFields; i++) {
        simpleDataPoint info = fields[i];
        
        uint32_t unsignedConstrained = formatValue(data[i], info.min, info.max); 
        copyValueToData(&unsignedConstrained, tempData, currBit, info.bits);
        currBit += info.bits;
    }

    ESP_LOGI(TAG, "\nsending data:");
    for(size_t i=0; i<tempBytes;i++){
        ESP_LOGI(TAG, "%0x", tempData[i]);
    
    }
}