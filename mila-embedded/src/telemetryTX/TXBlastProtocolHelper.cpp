#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>
#include <stdint.h>
#include <RadioLib.h>

#include "TXBlastProtocolHelper.hpp"
#include "../LoraCommon/LoraDriver.hpp"
#include "LoraTransmitQueue.hpp"
#include "LoraErrLog.hpp"

static const char* TAG = "Blast_Protocol_Helper";

static uint8_t  currentBurstIndex = 0;
static uint8_t burstBuffer[maxPacketGroupSize][maxLoraPacketSize];
static uint8_t burstBufferLens[maxPacketGroupSize];  //how many bytes in each burst buffer? (includes header)
static uint8_t burstBufferCount = 0;

static uint64_t transmitGiveUpTime = 0;

bool protocolGrab(){
    while(xSemaphoreTake(protocolMutex, portMAX_DELAY) != pdTRUE);
    if(!protocolRunning){   
        xSemaphoreGive(protocolMutex);
        return false;   //protocol not running, should exit!
    }
    return true;    //This is the sole thread allowed to do Lora Protocol stuff.
}

void protocolYield(){
    xSemaphoreGive(protocolMutex); //give mutex back
}

//returns true if protocol running, so a nudge attempt was valid
bool nudgeTransmission() {
    if(!protocolGrab()){
        return false;   //protocol not running
    }

    //if we arent doing anything, start a new burst
    if(!isBlasting && !awaitingAck){
        refreshBurstBuffer(burstBuffer, burstBufferLens, burstBufferCount);
        startNewBurstSequence();
    }

    protocolYield();
    return true;
}

/**
 * will try to start TX/RX a few times. If we fail, raise the issue to errorLog, which will reboot
 */
void safeProtocolTransmit(uint8_t* data, const uint16_t len, const uint32_t timeout_ms) {
    // Construct Header IN PLACE
    // Byte 0-1: Protocol ID
    data[0] = (uint8_t)(protocolUniqueID & 0xFF);
    data[1] = (uint8_t)(protocolUniqueID >> 8);
    // Byte 2: Sequence Number (High 4 bits: Index, Low 4 bits: Total Count - 1)
    data[2] = (currentBurstIndex << 4) | ((burstBufferCount - 1) & 0x0F);

    if(ackParity){
        data[3] |= ackParityMask;
    }
    int16_t state = LoraTransmit(data, len, timeout_ms);
    if(state != RADIOLIB_ERR_NONE){
        logErr(state);
    }
    if(state == RADIOLIB_LORA_DETECTED){
        //we timed out on TX, switch to RX
        listenForAck();
    }
}

void processBitmap(uint16_t bitmap) {
    uint8_t writeIdx = 0;
    for (uint8_t readIdx = 0; readIdx < burstBufferCount; readIdx++) {
        if (!((bitmap >> readIdx) & 0x01)) {
            if (writeIdx != readIdx) memcpy(burstBuffer[writeIdx], burstBuffer[readIdx], sizeof(burstBuffer[0]));
            writeIdx++;
        }
    }
    burstBufferCount = writeIdx;
    refreshBurstBuffer(burstBuffer, burstBufferLens, burstBufferCount);
}

void sendNextPacketInBurst() {
    // 1. Check bounds first
    if (currentBurstIndex >= burstBufferCount) {
        ESP_LOGI(TAG, "All packets in burst sent, listening for ACK");
        // We are done with this burst
        isBlasting = false;
        listenForAck();
        return;
    }

    safeProtocolTransmit(burstBuffer[currentBurstIndex], burstBufferLens[currentBurstIndex], transmitGiveUpTime);

    //increment the burst index
    currentBurstIndex++;
}


//tell RX side to stop we dont have any new packets to send
static inline void sendSilencer(){
    uint8_t dummy[5] = { (uint8_t)(protocolUniqueID & 0xFF), 
                            (uint8_t)(protocolUniqueID >> 8), 
                            0x00, 0x00, 0x01 };
    transmitGiveUpTime = esp_timer_get_time() + packetTimeOnAir_us;
    safeProtocolTransmit(dummy, 5, transmitGiveUpTime);
    LoraStartRecv();
}

void startNewBurstSequence() {
    if (burstBufferCount == 0) {
        sendSilencer();
        return;
    }
    currentBurstIndex = 0;
    isBlasting = true;
    transmitGiveUpTime = (esp_timer_get_time()) + (packetTimeOnAir_us * burstBufferCount + 1);  //allow how long we expect + 1 packet time on air
    sendNextPacketInBurst();
}


void listenForAck(){
    isBlasting = false;
    awaitingAck.store(true);
    ESP_LOGI(TAG, "timer started");
    xTimerStart(ackTimer, portMAX_DELAY);
    LoraStartRecv();
}