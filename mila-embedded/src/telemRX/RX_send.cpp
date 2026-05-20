#include <stdint.h>
#include <stddef.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "esp_timer.h"

#include "RXBlastProtocolHelper.hpp"
#include "../LoraCommon/LoraProtocol.h"
#include "../LoraCommon/blastProtocolConfig.hpp"
#include "../LoraCommon/Driver/Driver.hpp"
#include "../LoraCommon/safeDriverUtil.hpp"
#include "../LoraCommon/LoraErrLog.hpp"
// Define a tag for logging
static const char* TAG = "RX_send";

#include "RadioLib.h"

static SemaphoreHandle_t sendBufferMutex = NULL; //mutex to take or add to msg queue.

bool ackDataQueued = false;
RXProtocolPacket sendBuffer={};
int sendBufferDataIndex = 0;
uint8_t holdBuffer[maxRXDataSize] = {0};
int holdBufferDataIndex = 0;

void initSendBuffer(){
    if(sendBufferMutex == NULL){ 
        sendBufferDataIndex = 0; holdBufferDataIndex = 0;
        sendBufferMutex = xSemaphoreCreateMutex();
    }
}

//returns true if message is queue for transmission. false if no room.
bool protocol_Transmit(uint8_t* data, uint8_t dataLen){
    if (sendBufferMutex == NULL) return false;
    if(xSemaphoreTake(sendBufferMutex, pdMS_TO_TICKS(1000)) != pdPASS){
        ESP_LOGE(TAG, "Could not obtain send buffer mutex");
        return false;
    }
    if(holdBufferDataIndex + dataLen > sizeof(holdBuffer)){
        xSemaphoreGive(sendBufferMutex);
        return false;
    }
    memcpy(holdBuffer + holdBufferDataIndex, data, dataLen);
    holdBufferDataIndex += dataLen;
    xSemaphoreGive(sendBufferMutex);
    return true;
}

//for when ack recieved, clear data buffer
void flushSendBuffer(){
    ackDataQueued = false;
    sendBufferDataIndex = 0;
}

ProtocolState sendAck(){
    if(burstState.bitmap == 0){
        return awaitingData;    //nothing to ack
    }
    if(!ackDataQueued && holdBufferDataIndex > 0){   
        //move the hold buffer data to the send buffer.
        memcpy(sendBuffer.data, holdBuffer, holdBufferDataIndex); 
        sendBufferDataIndex = holdBufferDataIndex;
        holdBufferDataIndex = 0;
        ackDataQueued = true;
    }

    // Store the state of the completed burst so it can be queried by getBitmap
    burstState.last_sent_ack_bitmap = burstState.bitmap;
    burstState.last_sent_ack_burst_size = burstState.latestBurstSize + 1; // burstSize is 0-indexed, so +1 for count

    // Update header info in sendBuffer
    sendBuffer.protocolID = protocolUniqueID;
    sendBuffer.flags = burstState.recordedAckParity ? rxFlagMasks::ackParityMask : 0x00;
    sendBuffer.bitmap = burstState.bitmap;
    //
    //send the sendBuffer as a driverPacket with size
    driverSendPacket packet_to_send;
    packet_to_send.dataSize = RXHeaderSize + sendBufferDataIndex;
    packet_to_send.data = (uint8_t*) &sendBuffer;

    //ok to block for a very long time to get this packet queued, otherwise, will trigger crash:
    const uint64_t timeoutTime_us = esp_timer_get_time() + ((maxPacketGroupSize * packetTimeOnAir_us) * 8);
    result res = safeLoraTx(&packet_to_send, timeoutTime_us);
    switch(res){
        case Success:
            break; //keep going and wait for TX done
        case Timeout:
            ESP_LOGE(TAG, "Timed out trying to queue ACK for transmission.");
            raiseDriverCrash(TX_Queue_Msg_Timeout, "Timed out trying to queue ACK for transmission.");
            return crashed;
        case Crashed:
            ESP_LOGE(TAG, "Driver crashed while trying to send ACK.");
            return crashed;
        case Unknown:
            ESP_LOGE(TAG, "Unknown error while trying to send ACK.");
            return awaitingData;
    }

    ESP_LOGI(TAG, "sent ack for burst with bitmap: 0x%X", burstState.bitmap);
    return awaitingData;
}
