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
#include "../LoraCommon/safeDriverUtil.hpp"
#include "../LoraCommon/Driver/Driver.hpp"
#include "../LoraCommon/LoraErrLog.hpp"

static const char* TAG = "RX_AwaitData";

//allow user to view the bitmap  asynchronously (to help judge losses). not atomicity, so not necessarily accurate.
void getBitmap(uint16_t& bitmap, uint8_t& burstSize){
    bitmap = burstState.last_sent_ack_bitmap;
    burstSize = burstState.last_sent_ack_burst_size;
}

bool processBurst(driverRecvPacket* packet, uint64_t &timeoutDuration_us);

ProtocolState handleTimeout(bool recievedAnything){
    if(recievedAnything){
        return sendingAck;  //ack what we got
    }
    //got nothing for the large timeout duration
    raiseDriverCrash(RX_TIMEOUT, "awaitData timeout");
    return crashed;
}

ProtocolState awaitData(){
    uint64_t startTime = esp_timer_get_time();
    uint32_t timeout_duration_us = 5*(packetTimeOnAir_us * (maxPacketGroupSize+1)); //will trigger crash on timeout
    ESP_LOGI(TAG, "timeout duration: %d us", timeout_duration_us);
    uint64_t timerExpireTime_us = startTime + timeout_duration_us;
    bool recievedAnything = false;
    do{
        // timerExpireTime_us = startTime + timeout_duration_us;
        driverRecvPacket* packet;
        result res = safeWaitForRecv(packet, timerExpireTime_us);
        switch(res){
            case Crashed:
                return crashed;
            case Timeout:
                return handleTimeout(recievedAnything);
            case Unknown:
                continue;
            case Success:
                break;  //keep going and process this packet!
        }
        //basic header validation
        if(!validatePacketHeader(packet, TXHeaderSize)){
            ESP_LOGW(TAG, "invalid packet header");
            //invalid packet. keep trying to listen to burst
            continue;
        }
        recievedAnything = true;
        if(processBurst(packet, timerExpireTime_us)){
            //we recv the last packet. switch to sendingAck! :)
            return sendingAck;
        }
        //otherwise, keep trying to listen
    } while(esp_timer_get_time() < timerExpireTime_us);

    return handleTimeout(recievedAnything);
}

inline bool burstHeaderChanged(int recordedBurstSize, TXProtocolPacket* TXPacket){
    if(recordedBurstSize != FrameTrack::get_burstSize(TXPacket->frameNum)){
        ESP_LOGE(TAG, "burstSize changed un-expectdly, disregarding previous burst");
        return true;
    }
    return false;
}
inline bool frameNumDecreasedInBurst(int lastFrameRecv, int currentFrameNum){
    // if lastFrameRecv is -1, it's the first frame of a burst, so it's not a decrease.
    if(lastFrameRecv != -1 && currentFrameNum < lastFrameRecv) {
        ESP_LOGW(TAG,  "frame number within a packet went backward");
        return true;
    }
    return false;
}

//returns true if we are done parsing this burst
//false if packet is invalid, or  is in the middle
//updates the timeoutDuration if we are in the middle of the burst
bool processBurst(driverRecvPacket* packet, uint64_t &timerExpireTime_us){
    TXProtocolPacket* TXPacket = (TXProtocolPacket*)(packet->data);
    int currentFrameNum = FrameTrack::get_frameNum(TXPacket->frameNum);

    bool newBurstRecv = false;
    if (!burstState.paritySet) {
        // First packet since init/restart. Always start a new context.
        ESP_LOGI(TAG, "First packet received. Starting new burst context.");
        newBurstRecv = true;
    } else if ((TXPacket->flags & txFlagMasks::firstBurstMask) && !burstState.firstBurstEncounter) {
        // TX has restarted. This is a new burst.                  ^^that we havent already seen
        ESP_LOGW(TAG, "TX restart detected (firstBurst flag). Starting new burst context.");
        newBurstRecv = true;
    } else if (burstState.recordedAckParity != (TXPacket->flags & txFlagMasks::ackParityMask)) {
        // Normal new burst after successful ACK.
        ESP_LOGI(TAG, "Parity toggled. Starting new burst context.");
        newBurstRecv = true;
        flushSendBuffer(); // Only flus here, since can b confident TX got our ACK.
    } else if (burstHeaderChanged(burstState.latestBurstSize, TXPacket) || frameNumDecreasedInBurst(burstState.lastFrameRecv, currentFrameNum)) {
        // Unexpected change, likely from a missed packet that would have toggled parity. Treat it as a new burst
        newBurstRecv = true;
    }

    if(newBurstRecv){
        //if parity toggles or the header changes, they are sending on a new burst
        burstState.latestBurstSize = FrameTrack::get_burstSize(TXPacket->frameNum);
        burstState.recordedAckParity = (TXPacket->flags & txFlagMasks::ackParityMask) != 0;
        burstState.bitmap = 0;

        // Update firstBurstEncounter based on the flag in the new burst's first packet
        burstState.firstBurstEncounter = (TXPacket->flags & txFlagMasks::firstBurstMask) != 0;
    }
    burstState.paritySet = true;

    //handle this specific frame within the burst
    burstState.lastFrameRecv = currentFrameNum;
    ESP_LOGI(TAG, "got frame %d in burst of size: %d", burstState.lastFrameRecv, burstState.latestBurstSize);

    if(burstState.latestBurstSize == burstState.lastFrameRecv){
        //we got the last packet in the burst
        burstState.bitmap |= (1<<burstState.lastFrameRecv);
        xQueueSend(recvQueue, packet, portMAX_DELAY);   //send this packet to be parsed
        return true;
    }

    //otherwise, its a packet in the middle.
    timerExpireTime_us = esp_timer_get_time() + (packetTimeOnAir_us * (burstState.latestBurstSize - currentFrameNum + 1)); //how long until we expect this burst to be done
    burstState.bitmap |= (1<<burstState.lastFrameRecv);
    xQueueSend(recvQueue, packet, portMAX_DELAY);   //send this packet to be parsed
    return false;
}
