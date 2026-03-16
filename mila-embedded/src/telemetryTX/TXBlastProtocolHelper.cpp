#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>
#include <stdint.h>
#include <RadioLib.h>

#include "TXBlastProtocolHelper.hpp"
#include "../LoraCommon/blastProtocolConfig.hpp"
#include "LoraTransmitQueue.hpp"
#include "../LoraCommon/LoraErrLog.hpp"
#include "../LoraCommon/Driver/Driver.hpp"
#include "../LoraCommon/LoraProtocol.hpp"

static const char* TAG = "Blast_Protocol_Helper";

static uint8_t  currentBurstIndex = 0;
static uint8_t burstBuffer[maxPacketGroupSize][maxLoraPacketSize];
static uint8_t burstBufferLens[maxPacketGroupSize];  //how many bytes in each burst buffer? (includes header)
static uint8_t burstBufferCount = 0;

static uint64_t transmitGiveUpTime = 0;

ProtocolState sendData() {
    // 1. Check bounds first
    if(burstBufferCount < currentBurstIndex){
        ESP_LOGE(TAG, "attempted to transmit out of bounds frame");
        return awaitingAck;
    }
    TXProtocolPacket* responsePacket = (TXProtocolPacket*) burstBuffer[currentBurstIndex];
    responsePacket->dataSize = burstBufferLens[currentBurstIndex];
    responsePacket->protocolID = protocolUniqueID;
    responsePacket->frameNum = (currentBurstIndex << 4) | ((burstBufferCount - 1) & 0x0F);
    if(ackParity){
        responsePacket->flags |= txFlagMasks::ackParityMask;
    }

    if((LoraTransmit((driverPacket*) responsePacket, transmitGiveUpTime)) != RADIOLIB_ERR_NONE){
        ESP_LOGE(TAG, "transmission queue timed out");
        if(currentBurstIndex == 0){
            //if we didnt send anything, no point in trying to wait for ack
            return sendingData;
        }else{
            return awaitingAck;
        }
    }

    if(currentBurstIndex >= burstBufferCount){
        ESP_LOGI(TAG, "All packets in burst sent, listening for ACK");
        return awaitingAck;
    }
    //increment the burst index
    currentBurstIndex++;
    return 
}

ProtocolState resendLastPacketInBurst(){
    transmitGiveUpTime = INT64_MAX;     //no timeout on this one
    if(LoraTransmit((driverPacket*) (burstBuffer[burstBufferCount-1]), transmitGiveUpTime)){
        return crashed;
    }
    return awaitingAck;
}

//will block until a data is ready if none
ProtocolState prepareNewBurst(){
    //slot new stuff into burstBuffer
    refreshBurstBuffer(burstBuffer, burstBufferLens, burstBufferCount);
    currentBurstIndex = 0;
    transmitGiveUpTime = (esp_timer_get_time()) + (packetTimeOnAir_us * burstBufferCount + 1);  //allow how long we expect + 1 packet time on air
    return sendingData;
}

ProtocolState processAck(driverInfo* info);

ProtocolState awaitAck(){
    uint64_t startTime = esp_timer_get_time();
    uint32_t timeout_duration_us = (packetTimeOnAir_us * (1)); //give duration of one message to hear ack
    uint64_t timerExpireTime_us;
    do{
        timerExpireTime_us = startTime + timeout_duration_us;
        driverInfo* info = safeWaitForRecv(timerExpireTime_us);
        if(info == NULL){   //timed out
            enterStandBy(); //stop driver. want clean state when we switch to sending ack
            return resendLastData;   //timed out waiting for the packet
        }
        if(info->state == off){   //driver crashed, forward error to the user.
            return crashed;
        }

        if(!info->recvPacketReady){
            //unexpected interupt just triggered.
            enterStandBy(); //stop driver. want clean state. try again
            continue;
        }
        
        if(!validatePacketHeader(&(info->recvPacket))){
            //invalid packet. keep trying to listen to burst
            continue;
        }
        processAck(info);
        return sendingData;
        //otherwise, keep trying to listen
    } while(esp_timer_get_time() < timerExpireTime_us);
    //do not move to next burst
    return sendingData;
}

ProtocolState processAck(driverInfo* info) {
    RXProtocolPacket* packet = (RXProtocolPacket*) info;
    if((packet->flags & rxFlagMasks::ackParityMask) != ackParity){ 
        ESP_LOGE(TAG, "received outdated ack");
        logErr(recvOutdatedAck);
        return resendLastData;
    }
    uint8_t writeIdx = 0;
    rx_bitmap_t bitmap = packet->bitmap;
    //slide packets in the burst Buffer over. 
    for (uint8_t readIdx = 0; readIdx < burstBufferCount; readIdx++) {
        if (!((bitmap >> readIdx) & 0x01)) {
            if (writeIdx != readIdx) memcpy(burstBuffer[writeIdx], burstBuffer[readIdx], sizeof(burstBuffer[0]));
            writeIdx++;
        }
    }
    burstBufferCount = writeIdx;
    if(burstBufferCount == 0){
        //we recv all acks for packets in this burst
        return startNewBurst;
    }else{
        //we did not recv all packets, start again
        currentBurstIndex = 0;  //start from spot 0
        return sendingData;
    }
}
