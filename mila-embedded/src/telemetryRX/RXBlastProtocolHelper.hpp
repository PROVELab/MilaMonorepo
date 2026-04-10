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

struct burstStateContainer {
    bool recordedAckParity;
    bool paritySet;
    int latestBurstSize;
    int lastFrameRecv;
    bool firstBurstEncounter;
    rx_bitmap_t bitmap;
    rx_bitmap_t last_sent_ack_bitmap;
    uint8_t last_sent_ack_burst_size;
};
extern burstStateContainer burstState;
//wait on data
ProtocolState awaitData();
void flushSendBuffer();  //sent from awaitData to RX_send when recv ack.

//send data
void initSendBuffer();
ProtocolState sendAck();
#endif