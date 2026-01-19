#include <stdint.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <atomic>
#include <stdbool.h>

#include "../pecan/pecan.h"

#include "LoraTransmitQueue.hpp"
#include "LoraErrLog.hpp"
#include "TXBlastProtocolHelper.hpp"
#include "TXBlastProtocol.hpp"
#include "../LoraCommon/LoraDriver.hpp"

static const char* TAG = "Blast_Protocol";

// For receiving acks
uint32_t packetTimeOnAir_us = 1; //computed in initProtocol
TimerHandle_t ackTimer = NULL;
StaticTimer_t ackTimer_Buffer;      
std::atomic<bool> awaitingAck{0};
//we will first send with ackParity = false
bool ackParity = false; //parity bit to track acks. Ensure we know if we got old ack

//For blasting out of a buffer
bool isBlasting = false;

//ensure only one call to protocol running at a time
SemaphoreHandle_t protocolMutex = NULL; //not static since helper uses these
StaticSemaphore_t protocolMutexBuffer;
bool protocolRunning = false;
static bool protocolInitialized = false; 
//

// --- Prototypes ---
void startNewBurstSequence();
void ackTimeoutCallback(TimerHandle_t xTimer);

//can call again to re-initialize protocol
void initProtocol(const RadioConfig config) {
    protocolRunning = false;    //should already be false when called
    if(protocolMutex == NULL){
        protocolMutex = xSemaphoreCreateMutexStatic(&protocolMutexBuffer);
    }
    while(xSemaphoreTake(protocolMutex, portMAX_DELAY) != pdPASS);
    if(crashBinary == NULL){
        crashBinary = xSemaphoreCreateBinary();
    }
    
    //initialize things in other files. 
    initQueue();    //init and re-init are the same for these
    initErr();
    //anything whose init function cant also re-init is distinguished here
    if(!protocolInitialized){   
        LoraDriverInit(config);
    } else{
        LoraDriverRestart(config); 
    }
    //
    
    packetTimeOnAir_us = LoraGetTimeOnAir(); //compute time on air for max size packet

    //stuff for receiving acks
    awaitingAck.store(false);
    ESP_LOGI(TAG, "Packet Time on Air: %u us", packetTimeOnAir_us);
    if(ackTimer != NULL){
        ackTimer = xTimerCreateStatic("ackTimer",          //timer name, doesnt affect code execution
                            pdMS_TO_TICKS((packetTimeOnAir_us * 4) / 1000), //give 4 packets worth of time to here ack
                            pdFALSE,            //only fire one time (do not auto-renew)
                            (void*) NULL,      //parameter
                            ackTimeoutCallback, // the callback function
                            &(ackTimer_Buffer)  // buffer that holds timer info stuff
        );
    }
    
    protocolRunning = true;
    protocolInitialized = true;
    xSemaphoreGive(protocolMutex);
}

//to handle crashes. Declared extern so can be accessed by API file
SemaphoreHandle_t crashBinary = NULL;
int16_t driverCrashError = 0;
char driverCrashMsg [crashMsgSize] = {0};
//
void protocolCrash(const int16_t error, const char* msg){
    //grab protocol if we dont have it already
    if(xSemaphoreGetMutexHolder( protocolMutex ) != xTaskGetCurrentTaskHandle()){
        while (xSemaphoreTake(protocolMutex, portMAX_DELAY) != pdPASS);
    }

    //copy values of error
    memcpy(driverCrashMsg, msg, crashMsgSize - 1);
    driverCrashMsg[crashMsgSize-1] = 0 ;    //force it to be null terminated
    driverCrashError = error;

    xSemaphoreGive(protocolMutex); //give mutex back
    xSemaphoreGive(crashBinary);    //notify API of error
}

bool protocolTransmit(uint8_t* data, uint8_t dataLen){
    addFrameToQueue(data, dataLen);
    return nudgeTransmission();
}

//not implementing for now
// bool protocolTransmitPriority(uint8_t* data, uint8_t dataLen){
//
// }
// --- Logic Callbacks ---

void ackTimeoutCallback(TimerHandle_t xTimer) {  //arg = SX1262_EXT* radio
    if(!protocolGrab()){
        return;
    }
    if(awaitingAck.exchange(false) == true){   //we were awaiting Ack
        if(!isBlasting){
            logErr(ackTimeout);
            ESP_LOGW(TAG, "ACK Timeout! Retrying...");
            startNewBurstSequence();
        }
    }
    protocolYield();
}

void protocolTXComplete() {
    if(!protocolGrab()){
        return; //protocol no longer running
    }
    if (isBlasting) {
        sendNextPacketInBurst();
    } else {
        // If we received an interrupt we weren't expecting
        logErr(unexpectedTXCompletion);
    }
    protocolYield();
}

void protocolReceive(const uint8_t* rxBuffer, size_t length) {
    if(!protocolGrab()){
        return; //protocol no longer running
    }
    //cancel timeout
    if(awaitingAck.exchange(false) == false){  //grab control
        protocolYield();
        return; //the timeout already fired, or we arent expecting a recv
    }
    xTimerStop(ackTimer, portMAX_DELAY);    //stop timer

    bool packetErr = false;

    //Error checking
    if (length < 7) {   //basic length check
        logErr(invalidRXLength);
        packetErr = true;
    }
    //check our protocalID should be first 2 bytes
    uint16_t recvID = rxBuffer[0] | (rxBuffer[1] << 8);
    if (length < 2 || ((recvID & protocolUniqueIDMask) != protocolUniqueID)) {
        logErr(incorrectProtocolId);
        packetErr = true;
    }
    if(packetErr){
        protocolYield();
        return;
    }

    if((recvID & parityBitMask) != ackParity){ 
        ESP_LOGE(TAG, "received outdated ack");
        //we got sent an old ack
        //try again with same as b4. dont move forward
        startNewBurstSequence(); 
        protocolYield();
        return;
    }

    // 3. Branching: Process or Re-arm
    // --- SUCCESS CASE ---
    ESP_LOGI(TAG, "Valid Bitmap Received.");
    uint16_t bitmap = *((uint16_t*)(rxBuffer + 5));
    processBitmap(bitmap);
    ackParity = !ackParity; //flip parity for next ack

    startNewBurstSequence();
    protocolYield();    
}

