#include <stdint.h>
#include <stdio.h>
#include <cstring>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "RadioLib.h"

#include "RXBlastProtocolHelper.hpp"
#include "../LoraCommon/LoraProtocol.h"
#include "../LoraCommon/blastProtocolConfig.hpp"
#include "../LoraCommon/safeDriverUtil.hpp"
#include "../LoraCommon/Driver/Driver.hpp"
#include "../LoraCommon/LoraErrLog.hpp"

static const char* TAG = "RX_AwaitData";

//allow user to view the bitmap  asynchronously (to help judge losses). no atomicity, so not necessarily accurate.
void getBitmap(uint16_t* bitmap, uint8_t* burstSize){
    *bitmap = burstState.last_sent_ack_bitmap;
    *burstSize = burstState.last_sent_ack_burst_size;
}

bool processBurst(driverRecvPacket* packet, uint64_t &timeoutDuration_us);

ProtocolState handleTimeout(bool receivedAnything){
    if(receivedAnything){
        return sendingAck;  //ack what we got
    }
    //got nothing for the large timeout duration
    raiseDriverCrash(RX_TIMEOUT, "awaitData timeout");
    return crashed;
}

ProtocolState awaitData(){
    uint64_t startTime = esp_timer_get_time();
    uint32_t timeout_duration_us = 5*(packetTimeOnAir_us * (maxPacketGroupSize+1)); //will trigger crash on timeout
    ESP_LOGI(TAG, "timeout duration: %" PRIu32 " us", timeout_duration_us);
    uint64_t timerExpireTime_us = startTime + timeout_duration_us;
    bool receivedAnything = false;
    do{
        // timerExpireTime_us = startTime + timeout_duration_us;
        driverRecvPacket* packet;
        result res = safeWaitForRecv(packet, timerExpireTime_us);
        switch(res){
            case Crashed:
                return crashed;
            case Timeout:
                return handleTimeout(receivedAnything);
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
        receivedAnything = true;
        if(processBurst(packet, timerExpireTime_us)){
            //we recv the last packet. switch to sendingAck! :)
            return sendingAck;
        }
        //otherwise, keep trying to listen
    } while(esp_timer_get_time() < timerExpireTime_us);

    return handleTimeout(receivedAnything);
}

inline bool frameNumDecreasedInBurst(int lastFrameRecv, int currentFrameNum){
    // if lastFrameRecv is -1, it's the first frame of a burst, so it's not a decrease.
    if(lastFrameRecv != -1 && currentFrameNum < lastFrameRecv) {
        ESP_LOGW(TAG,  "frame number within a packet went backward");
        return true;
    }
    return false;
}

inline bool newBurstReceived(TXProtocolPacket& TXPacket, const int currentFrameNum){
    if (!burstState.paritySet) {
        // First packet since init/restart. Always start a new context.
        ESP_LOGI(TAG, "First packet received. Starting new burst context.");
        return true;
    }
    if ((TXPacket.flags & txFlagMasks::firstBurstMask) && !burstState.firstBurstEncounter) {
        // TX has restarted. This is a new burst.                  ^^that we havent already seen
        ESP_LOGW(TAG, "TX restart detected (firstBurst flag). Starting new burst context.");
        return true;
    } 
    if (burstState.recordedAckParity != (TXPacket.flags & txFlagMasks::ackParityMask)) {
        // Normal new burst after successful ACK.
        ESP_LOGI(TAG, "Parity toggled. Starting new burst context.");
        flushSendBuffer(); // Only flush here, since can now be confident TX got our last msg.
        return true;
    }
    if(burstState.latestBurstSize != FrameTrack::get_burstSize(TXPacket.frameNum)){
        ESP_LOGI(TAG, "burstSize changed un-expectedly, disregarding previous burst");
        return true; //likely missed ack that would cause a parity toggle
    }
    if(frameNumDecreasedInBurst(burstState.lastFrameRecv, currentFrameNum)) {
        ESP_LOGI(TAG, "unexpected frame num decrease");
        return true; //Likely missed ack that would cause a parity toggle
    }
    return false;
}

//returns true if we are done parsing this burst
//false if packet is invalid, or  is in the middle
//updates the timerExpireTime_us if we are in the middle of the burst
bool processBurst(driverRecvPacket* packet, uint64_t &timerExpireTime_us){
    TXProtocolPacket TXPacket; 
    std::memcpy(&TXPacket, packet->data, packet->dataSize); //strict aliasing cringe
    int currentFrameNum = FrameTrack::get_frameNum(TXPacket.frameNum);
    if(newBurstReceived(TXPacket, currentFrameNum)){
        //if parity toggles or the header changes, they are sending on a new burst
        burstState.latestBurstSize = FrameTrack::get_burstSize(TXPacket.frameNum);
        burstState.recordedAckParity = (TXPacket.flags & txFlagMasks::ackParityMask) != 0;
        burstState.bitmap = 0;

        // Update firstBurstEncounter based on the flag in the new burst's first packet
        burstState.firstBurstEncounter = (TXPacket.flags & txFlagMasks::firstBurstMask) != 0;
    }

    burstState.paritySet = true; // have recv at least one packet, so parity is up to date.

    //handle this specific frame within the burst
    burstState.lastFrameRecv = currentFrameNum;
    ESP_LOGI(TAG, "got frame %d in burst of size: %d", burstState.lastFrameRecv, burstState.latestBurstSize);
    xQueueSend(recvQueue, packet, 0);   //send this packet to be parsed

    if(burstState.latestBurstSize == burstState.lastFrameRecv){
        //we got the last packet in the burst
        burstState.bitmap |= (1<<burstState.lastFrameRecv);
        return true; //done parsing this burst
    }

    //otherwise, its a packet in the middle.
    timerExpireTime_us = esp_timer_get_time() 
                        + (packetTimeOnAir_us * (burstState.latestBurstSize - currentFrameNum));
                        //how much time until we expect this burst to be done.
    burstState.bitmap |= (1<<burstState.lastFrameRecv);
    return false; //not done parsing this burst
}
