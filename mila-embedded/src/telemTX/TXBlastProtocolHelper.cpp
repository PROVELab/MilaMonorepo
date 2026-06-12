#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>
#include <cstring>
#include <stdint.h>
#include <RadioLib.h>

#include "TXBlastProtocolHelper.hpp"
#include "../LoraCommon/blastProtocolConfig.hpp"
#include "LoraTransmitQueue.hpp"
#include "../LoraCommon/LoraErrLog.hpp"
#include "../LoraCommon/Driver/Driver.hpp"
#include "../LoraCommon/LoraProtocol.h"
#include "../LoraCommon/safeDriverUtil.hpp"

static const char* TAG = "Blast_Protocol_Helper";

static uint8_t  currentBurstIndex = 0;
static TXProtocolPacket burstBuffer[maxPacketGroupSize];
static uint8_t burstBufferCount = 0;
static size_t burstPacketSizes[maxPacketGroupSize]; // Tracks the actual size (header + payload) for each packet in burstBuffer
static uint8_t originalBurstBufferCount = 0;

static uint64_t transmitGiveUpTime = 0;

ProtocolState handleTXTimeout(){
    ESP_LOGE(TAG, "transmission queue timed out waiting for clear channel");
    if(currentBurstIndex == 0){
        vTaskDelay(pdMS_TO_TICKS(30));
        return sendingData;
    }else{
        // Some packets were sent, so try to get an ACK for them.
        return awaitingAck;
    }
}
ProtocolState sendData() {
    if(currentBurstIndex >= burstBufferCount){
        ESP_LOGI(TAG, "All packets in burst sent, transitioning to await ACK");
        return awaitingAck;
    }

    // Create a temporary driverSendPacket and copy the data for transmission
    driverSendPacket packet_to_send;
    packet_to_send.dataSize = burstPacketSizes[currentBurstIndex];
    packet_to_send.data = (uint8_t*) &burstBuffer[currentBurstIndex];
    result res = safeLoraTx(&packet_to_send, transmitGiveUpTime);
    switch(res){
        case Crashed:
            ESP_LOGE(TAG, "Driver crashed while trying to resend packet %d/%d.", currentBurstIndex + 1, originalBurstBufferCount);
            return crashed;
        case Timeout:
            return handleTXTimeout();
        case Success:
            break; // continue with normal flow
        default:
            ESP_LOGW(TAG, "Unknown result from safeLoraTx while resending last packet.");
            return awaitingAck;
    }

    ESP_LOGI(TAG, "Packet %d/%d sent successfully.", currentBurstIndex + 1, originalBurstBufferCount);
    currentBurstIndex++;
    if (currentBurstIndex >= burstBufferCount) {
        return awaitingAck; // This was the last packet, now wait for ACK
    }
    return sendingData; // More packets to send
}

ProtocolState resendLastPacketInBurst(){
    ESP_LOGI(TAG, "Resending last packet to prompt for ACK.");
    transmitGiveUpTime = INT64_MAX;     //no timeout on this one

    if (originalBurstBufferCount == 0) {
        ESP_LOGE(TAG, "Cannot resend last packet, no burst was ever prepared.");
        return startNewBurst;
    }

    uint8_t lastPacketIndex = originalBurstBufferCount - 1;
    
    // Create a temporary driverSendPacket and copy the data for transmission
    driverSendPacket packet_to_send;
    packet_to_send.dataSize = burstPacketSizes[lastPacketIndex];
    packet_to_send.data = (uint8_t*) &burstBuffer[lastPacketIndex];

    result res = safeLoraTx(&packet_to_send, transmitGiveUpTime);
    switch(res){
        case Crashed:
            ESP_LOGE(TAG, "Driver crashed while trying to resend last packet.");
            return crashed;
        case Timeout:
            ESP_LOGE(TAG, "Timed out trying to resend last packet.");
            return awaitingAck;
        case Success:
            ESP_LOGI(TAG, "Last packet resent successfully.");
            return awaitingAck;
        default:
            ESP_LOGW(TAG, "Unknown result from safeLoraTx while resending last packet.");
            return awaitingAck;
    }
}

// moves packets from queue into burst Buffer. Slides un-acked 
//will block until a data is ready if none in burstBuffer or queue
ProtocolState prepareNewBurst(){
    // Fills the rest of the burst buffer with new packets from the queue.
    // If burstBufferCount is 0, this fills a completely new burst.
    // If burstBufferCount > 0, this appends to the existing un-acked packets.
    
    // The `refreshBurstBuffer` function needs to know where to start filling.
    burstBufferCount = queue.refreshBurstBuffer(burstBuffer, burstPacketSizes, burstBufferCount);
    originalBurstBufferCount = burstBufferCount;

    // Re-header all packets in the newly formed burst to ensure consistency
    for (uint8_t i = 0; i < originalBurstBufferCount; i++) {
        burstBuffer[i].protocolID = protocolUniqueID;
        burstBuffer[i].frameNum = 0; // Zero out before setting
        FrameTrack::set_burstSize(burstBuffer[i].frameNum, originalBurstBufferCount - 1);
        FrameTrack::set_frameNum(burstBuffer[i].frameNum, i);
        burstBuffer[i].flags &= txFlagMasks::priorityPacketMask; //clear everything but the priority mask
        burstBuffer[i].flags |= (ackParity ? txFlagMasks::ackParityMask : 0) | (firstBurst ? txFlagMasks::firstBurstMask : 0);
    }
    currentBurstIndex = 0;
    transmitGiveUpTime = (esp_timer_get_time()) + (packetTimeOnAir_us * (burstBufferCount + 1));  //allow how long we expect + 1 packet time on air
    return sendingData;
}

ProtocolState processAck(driverRecvPacket* driverPacket);

ProtocolState awaitAck(){
    const uint64_t startTime = esp_timer_get_time();
    const uint32_t timeout_duration_us = (packetTimeOnAir_us * (8)); //give duration of three messages to hear ack
    uint64_t timerExpireTime_us = startTime + timeout_duration_us;

    do{
        ESP_LOGI(TAG, "waiting for ack");
        // (const char* TAG, result& result, const driverPacket* packet, const uint64_t timerExpireTime_us)
        driverRecvPacket* packet;
        result res = safeWaitForRecv(packet, timerExpireTime_us);
        switch (res){
            case Crashed:
                return crashed;
            case Timeout:
                ESP_LOGI(TAG, "ack wait timed out");
                return resendLastData;   //timed out waiting for the packet
            case Unknown:
                continue;
            case Success:
                break;  //keep going and process this packet!
        }        
        if(!validatePacketHeader(packet, RXHeaderSize)){
            ESP_LOGW(TAG, "invalid packet header");
            //invalid packet. keep trying to listen to burst
            continue;
        }
        // processAck determines the next state based on the bitmap
        return processAck(packet);
        //otherwise, keep trying to listen
    } while(esp_timer_get_time() < timerExpireTime_us);
    // Timed out waiting for a valid ACK packet, resend last packet to prompt another ACK
    ESP_LOGI(TAG, "ack wait timed out");
    return resendLastData;
}

ProtocolState processAck(driverRecvPacket* driverPacket) {
    RXProtocolPacket packet;
    std::memcpy(&packet, driverPacket->data, RXHeaderSize);

    if((packet.flags & rxFlagMasks::ackParityMask) != ackParity){ 
        ESP_LOGE(TAG, "received outdated ack");
        logErr(TAG, recvOutdatedAck);
        return resendLastData;
    }
    uint8_t writeIdx = 0;
    rx_bitmap_t bitmap = packet.bitmap;
    // Slide un-acked packets to the front of the buffer for retransmission.
    // CRITICAL: The loop must iterate over the size of the burst that was *originally sent*,
    // which is stored in originalBurstBufferCount. Using burstBufferCount here can lead to state
    // corruption if a delayed ACK for a different-sized burst is received.
    for (uint8_t readIdx = 0; readIdx < originalBurstBufferCount; readIdx++) {
        if (!((bitmap >> readIdx) & 0x01)) { // If this packet was NOT acknowledged...
            if (writeIdx != readIdx) {
                burstBuffer[writeIdx] = burstBuffer[readIdx];
                burstPacketSizes[writeIdx] = burstPacketSizes[readIdx];
            }
            writeIdx++;
        }
    }
    burstBufferCount = writeIdx;

    ackParity = !ackParity; //toggle parity to indicate we got the ack
    firstBurst = false;

    //relay the msg to recv queue:
    if(xQueueSend(recvQueue, driverPacket, pdMS_TO_TICKS(1000)) != pdPASS){
        ESP_LOGW(TAG, "Failed to send received ACK to recvQueue");
    }

    return startNewBurst;
}
