#include <stdint.h>
#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "RadioLib.h"

#include "RXBlastProtocolHelper.hpp"
#include "../LoraCommon/LoraProtocol.hpp"
#include "../LoraCommon/blastProtocolConfig.hpp"
#include "../LoraCommon/Driver/Driver.hpp"

static const char* TAG = "RX_AwaitData";

bool processBurst(driverInfo* info, uint32_t &timeoutDuration_us);

ProtocolState awaitData(){
    uint64_t startTime = esp_timer_get_time();
    uint32_t timeout_duration_us = (packetTimeOnAir_us * (maxPacketGroupSize+1)); //give the full duration of a large burst of wait for the TX side
    uint64_t timerExpireTime_us;
    do{
        timerExpireTime_us = startTime + timeout_duration_us;
        driverInfo* info = safeWaitForRecv(timerExpireTime_us);
        if(info == NULL){
            enterStandBy(); //stop driver. want clean state when we switch to sending ack
            return sendingAck;   //timed out waiting for the packet
        }
        if(info->state == off){   //driver crashed, forward error to the user.
            return crashed;
        }

        if(!info->recvPacketReady){
            //unexpected interupt just triggered.
            enterStandBy(); //stop driver. want clean state
            continue;
        }
        
        if(!validatePacketHeader(&(info->recvPacket))){
            //invalid packet. keep trying to listen to burst
            continue;
        }

        if(processBurst(info, timeout_duration_us)){
            //we recv the last packet. switch to sendingAck! :)
            return sendingAck;
        }
        //otherwise, keep trying to listen
    } while(esp_timer_get_time() < timerExpireTime_us);
    return sendingAck;
}

inline bool burstHeaderChanged(int recordedBurstSize, int recordedAckParity, TXProtocolPacket* TXPacket){
    if(recordedBurstSize != FrameTrack::get_burstSize(TXPacket->frameNum)){
        ESP_LOGE(TAG, "burstSize changed un-expectdly, disregarding previous burst");
        return true;
    }
    if(TXPacket->flags & txFlagMasks::ackParityMask){
        ESP_LOGE(TAG, "ackParity changed unexpectdly, disregarding previous burst");
        return true;
    }
    return false;
}
inline bool frameNumDecreasedInBurst(int lastFrameRecv, int currentFrameNum){
    if(currentFrameNum < lastFrameRecv){
        ESP_LOGW(TAG,  "frame number within a packet went backward");
        return true;
    }
    return false;
}
//returns true if we are done parsing this burst
//false if packet is invalid, or  is in the middle
//updates the timeoutDuration if we are in the middle of the burst
bool processBurst(driverInfo* info, uint32_t &timeoutDuration_us){
    static int lastFrameRecv = 0;    //-1 for value not yet set (havent gotten a frame yet)
    static int burstSize = 0;     //track how many frames we expect in this burst, which we get from the first packet
    
    TXProtocolPacket* TXPacket = (TXProtocolPacket*)(&(info->recvPacket));

    if(!paritySet || (recordedAckParity != (TXPacket->flags & txFlagMasks::ackParityMask))
        || (burstHeaderChanged(burstSize, recordedAckParity, TXPacket)) 
        || (frameNumDecreasedInBurst(lastFrameRecv, FrameTrack::get_frameNum(TXPacket->frameNum)))
    ){
        //if parity toggles or the header changes, they are sending on a new burst
        burstSize = FrameTrack::get_burstSize(TXPacket->frameNum);
        recordedAckParity = TXPacket->flags & txFlagMasks::ackParityMask;
        responsePacket.bitmap = 0;
    }
    lastFrameRecv = FrameTrack::get_frameNum(TXPacket->frameNum);

    paritySet = true;
    
    //header looks ok
    if(burstSize == lastFrameRecv + 1){
        //we got the last packet in the burst (yay!)
        responsePacket.bitmap |= (1<<lastFrameRecv);
        xQueueSend(recvQueue, &(info->recvPacket), portMAX_DELAY);   //send this packet to be parsed
        return true;
    }

    //otherwise, its a packet in the middle.
    timeoutDuration_us = (packetTimeOnAir_us * (burstSize - lastFrameRecv + 1)); //how long until we expect this burst to be done
    responsePacket.bitmap |= (1<<lastFrameRecv);
    xQueueSend(recvQueue, &(info->recvPacket), portMAX_DELAY);   //send this packet to be parsed
    return false;
}
