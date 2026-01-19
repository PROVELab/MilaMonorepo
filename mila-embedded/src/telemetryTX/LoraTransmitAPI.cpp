#include <stdint.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include "../pecan/pecan.h"
#include "TXBlastProtocol.hpp"
#include "LoraTransmitAPI.hpp"
#include "LoraErrLog.hpp"

static const char* TAG = "LORA_API";

//**** Protocol Format **** /
//For protocol, packets are sent as:
//byte 0 and 1
//11 bit ID,
//4 bit dataSize (can have size up to 15 bits for OversizeFrame)
//1 bit <next Byte is extendedID?>
//byte 2
//If extendedID specified:
    //7 bits extendedID
    //1 bit Mnext Byte is extendedID?>  //can have up to 3 7 bit extendedID chunks
//byte 3-5 (depending on extID specificaiton)

//**** Why Format like this? ****/
//(Why is ExtID a LL?) ExtID is 18 bits, we still need at most 3 bytes for it. Since most our extIDs
//may be small, this lets us only transmit one extra byte (7 bits effectively) for them.
//If a packet is extended, and extID is 0, that byte is still transmited, just with value 0.

//(Why no RTR?) Post-pecan, all RTR packets have dataSize = 0, all other packets have dataSize != 0.
//For whatever reason, the Aruino Shield + sandeep library one way or another ignores packets with dataSize =0,
//Pecan uses RTR to denote when it really wants dataSize = 1, so RTR not actually used.

uint8_t parseExtIDSize(uint32_t id, uint32_t& extID){
    extID = getIdExtension(id);
    if(extID == 0){
        return 0;   //no extID
    }
    if(extID <= 0x7F){  //fits in 7 bits
        return 1;
    }
    if (extID <= 0x3FFF){   //fits in 14 bits
        return 2;
    }else{ //fits in 21 bits
        return 3;
    }
}
#define maxCanHeaderSize 5 
#define maxCanData 8
#define maxLoraData 15
#define maxLoraFrameSize (maxCanHeaderSize + maxLoraData)

static inline void writeID(uint32_t id, uint8_t dataLen, uint8_t*& writePtr){
    uint32_t extID;
    int8_t extIDSize = parseExtIDSize(id, extID);
    writePtr[0] = id & 0xFF;   //11 bits of ID
    writePtr[1] = ((id >> 8 ) & 0x7) | ((dataLen & 0xF) << 3); 
                                        //4 bit dataLen
    if(extIDSize > 0){
        writePtr[1] |= 0x80;   //set extendedID bit
    }
    writePtr += 2;
    //write extID
    for(; extIDSize > 0; writePtr++){
        *writePtr = extID & 0x7F;
        extID >>= 7;
        extIDSize--;
        if(extIDSize > 0){
            *writePtr |= 0x80; //set next byte bit
        }
    }
}

//non-reentrant
bool protocolTransmitCANFrame(CANPacket* packet){
    static uint8_t dataBufferCAN [maxCanData + maxCanHeaderSize];
    if(packet->dataSize > maxCanData){
        ESP_LOGI(TAG, "cant read data > 8 out of CANPacket, ur dataSize is %d", packet->dataSize);
        return false;
    }
    uint8_t* writePtr = dataBufferCAN;
    writeID(packet->id, packet->dataSize, writePtr);
    //write data
    memcpy(writePtr, packet->data, packet->dataSize);
    writePtr += packet->dataSize;
    uint8_t frameSize = (uint8_t)(writePtr - dataBufferCAN);
    return protocolTransmit(dataBufferCAN, frameSize);}

//For transmiting fabricated CanPackets with data length up to 15.
void protocolTransmitOversizeFrame(uint32_t id, uint8_t* data, int8_t dataLen){
    static uint8_t dataBufferOversize [maxLoraFrameSize];
    if(dataLen > maxLoraData){
        ESP_LOGI(TAG, "cant read data > 15 for OversizeFrame, ur dataLen is %d", dataLen);
        return;
    }
    uint8_t* writePtr = dataBufferOversize;
    writeID(id, dataLen, writePtr);
    //write data
    memcpy(writePtr, data, dataLen); //skip first 2 bytes (ID)
    writePtr += dataLen;
    uint8_t frameSize = (uint8_t)(writePtr - dataBufferOversize);
    protocolTransmit(dataBufferOversize, frameSize);
}

void Lora_TX_Init(const RadioConfig config){
    initProtocol(config);
}

void Lora_TX_Restart(const RadioConfig config){
    initProtocol(config);
}

//return error code
int16_t Lora_Monitor_Crash(char*& errorMsg, uint8_t& msgLen){
    //block until driver crashes
    while(xSemaphoreTake(crashBinary, portMAX_DELAY) != pdPASS);  
    //assign values set in LoraErrLog when it gives binary
    errorMsg = driverCrashMsg; 
    msgLen = strlen(errorMsg);
    return driverCrashError;
}

