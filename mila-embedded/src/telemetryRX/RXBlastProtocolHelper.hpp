#ifndef RX_BLAST_PROTOCOL_HELPER_H
#define RX_BLAST_PROTOCOL_HELPER_H

#include "../LoraCommon/blastProtocolConfig.hpp"


typedef enum{
    unStarted = 0,
    crashed = 1,
    awaitingData = 2,
    sendingAck = 3
} ProtocolState;

extern uint32_t packetTimeOnAir_us ;
extern QueueHandle_t recvQueue;
extern RXProtocolPacket responsePacket;
extern bool paritySet; //reset on init, set true the first time we recv a packet
extern bool recordedAckParity;

ProtocolState awaitData();
#endif