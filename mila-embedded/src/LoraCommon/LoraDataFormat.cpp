// #include <stdint.h>
// #include "esp_log.h"
// #include "freertos/FreeRTOS.h"

// #include "../pecan/pecan.h" //for CANPacket
// #include "LoraProtocol.hpp"   

// static const char* TAG = "LORA_FORMAT";


// //**** Packet Formatting **** /
// // 11 bit ID, 4 bit Datalength, 21 bit ext id, 
// //              ^<optionall>, bytes as a LL. 1 bit minimum to indicate end of LL
// #define maxCanHeaderSize 5 //11 bit std-id + 21 bit ext-id + 4 bit dataLen = 36, ciel(36/8) = 5 bytes
// #define maxCanToLoraData 15
// #define maxLoraFrameSize (maxCanHeaderSize + maxCanToLoraData)

// static bool ParseHeader(const uint8_t*& buffer, uint8_t& dataLen, uint32_t& CanID, const uint8_t* bufferEnd);

// //returns true if successfully parsed packet
// bool Lora_Format_Recv(uint32_t& id, uint8_t*& data, uint8_t& dataLen){
//     static driverPacket msg = {};
//     static const uint8_t* msgPtr = msg.data;
//     static const uint8_t* msgEnd = msg.data;

//     if(msgPtr >= msgEnd){

//         if(!clientRecv(&msg)){
//             return false;
//         }

//         msgPtr = msg.data;
//         msgEnd = msg.data + msg.dataSize;
//     }
//     //parses the header, and updates msgPtr to point at data
//     if(ParseHeader(msgPtr, dataLen, id, msgEnd)){
//         // Trust the header, unless data would go over bounds of data frame
//         if(msgPtr + dataLen > msgEnd){
//             msgPtr = msgEnd; //indicate end of packet
//             return false;
//         }
//         data = (uint8_t*)msgPtr;
//         msgPtr += dataLen;
//         return true;
//     }

//     msgPtr = msgEnd;    //we failed on last parse call, stop trying to parse this msg
//     return false;
// }

// //moves buffer to start of data section, sets dataLen and CanID.
// static bool ParseHeader(const uint8_t*& buffer, uint8_t& dataLen, uint32_t& CanID, const uint8_t* bufferEnd){
//     if(buffer + 2 > bufferEnd) return false;    //header is at least 2 bytes
//     uint16_t requiredHeader = buffer[0] | (buffer[1] << 8);
//     CanID = requiredHeader & 0x7FF;    //standard part of ID
//     uint8_t IDbitPos = 11;                      //start at end of standard ID
//     dataLen = (requiredHeader >> IDbitPos) & 0x0F;  //grab the data length
//     buffer++;

//     while(*buffer & 0x80){  //while the extendedID bit is true
//         buffer++;
//         if ( buffer == bufferEnd ) return false; //were going over the edge
//         CanID |= ((uint32_t)(*buffer & 0x7F)) << IDbitPos;
//         IDbitPos += 7;
//     }
//     buffer++;
//     return true;
// }

// //** Thread Safe **//
// static uint8_t* writeCANToLoraID(uint32_t id, uint8_t dataLen, uint8_t* writePtr);

// bool Lora_API_TransmitCANFrame(CANPacket* packet, bool highPriority){
//     uint8_t dataBufferCAN [MAX_SIZE_PACKET_DATA + maxCanHeaderSize];
//     if(packet->dataSize > MAX_SIZE_PACKET_DATA){
//         ESP_LOGI(TAG, "cant read data > 8 out of CANPacket, ur dataSize is %d", packet->dataSize);
//         return false;
//     }
//     uint8_t* writePtr = dataBufferCAN;
//     writePtr = writeCANToLoraID(packet->id, packet->dataSize, writePtr);
//     //write data
//     memcpy(writePtr, packet->data, packet->dataSize);
//     writePtr += packet->dataSize;
//     uint8_t frameSize = (uint8_t)(writePtr - dataBufferCAN);
//     return protocolTransmit(dataBufferCAN, frameSize);}

// //For transmiting fabricated CanPackets with data length up to 15.
// bool Lora_API_TransmitOversizeFrame(uint32_t id, uint8_t* data, int8_t dataLen, bool highPriority){
//     uint8_t dataBufferOversize [maxLoraFrameSize];
//     if(dataLen > maxCanToLoraData){
//         ESP_LOGI(TAG, "cant read data > %d for OversizeFrame, ur dataLen is %d", maxCanToLoraData, dataLen);
//         return false;
//     }
//     uint8_t* writePtr = dataBufferOversize;
//     writePtr = writeCANToLoraID(id, dataLen, writePtr);
//     //write data
//     memcpy(writePtr, data, dataLen); //skip first 2 bytes (ID)
//     writePtr += dataLen;
//     uint8_t frameSize = (uint8_t)(writePtr - dataBufferOversize);
//     return protocolTransmit(dataBufferOversize, frameSize);
// }

// static uint8_t* writeCANToLoraID(uint32_t id, uint8_t dataLen, uint8_t* writePtr){
//     //first two bytes:        11 bits id   4 bits dataLen
//     *((uint16_t*)writePtr) = id & 0x7FF | ((dataLen & 0xF) << 11); 

//     writePtr++;
//     id >>= 11;  //shift out normal ID
//     while(id > 0){
//         *writePtr |= 0x80;   //set bit 7 of last byte to indicate extending id
//         writePtr++;          //increment to next byte where this extension goes
//         *writePtr = id & 0x7F;  //write 7 bits of id into the next byte
//         id >>= 7;
//     }
//     writePtr++;
//     return writePtr;
// }