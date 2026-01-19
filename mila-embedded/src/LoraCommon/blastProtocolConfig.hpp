#include "LoraDriverConfig.hpp"   //maxLoraPacketSize
//queue & packet size of stuff
#define maxPacketGroupSize 8
#define maxPacketsInQueue 64

//header
#define protocolID_t uint16_t       //2 byte protocol ID
#define protocolUniqueID 0x9354
#define protocolUniqueIDMask 0xFFFE
#define parityBitMask 0x0001
//^The last bit of this field is used as a parity bit for acks.
#define protocolIDIndex 0
#define TXFrameNumIndex 1
#define TXFlagsIndex 2
#define ackParityMask 1
#define errorPacketMask 2
#define priorityPacketMask 4
//no other flags currently used 
#define headerSize (sizeof(protocolID_t) + 1)   //our burst size and stuff
#define protocolPacketDataBytes (maxLoraPacketSize - headerSize)  //how many bytes of data per packet
