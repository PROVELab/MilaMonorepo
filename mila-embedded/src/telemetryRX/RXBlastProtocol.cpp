#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "RXBlastProtocol.hpp"
#include "../LoraCommon/LoraDriver.hpp"
// Define a tag for logging
static const char* TAG = "RX_Blast";

//queue for incomming packets
#define queueSize 8
QueueHandle_t recvQueue = NULL;
StaticQueue_t xQueueBuffer;
uint8_t ucQueueStorage[ queueSize * sizeof(packetInfo)];
//

SemaphoreHandle_t protocolMutex = NULL; //not static since helper uses these
StaticSemaphore_t protocolMutexBuffer;
bool protocolRunning = false;

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

void initProtocol(const RadioConfig config){

    if(protocolMutex == NULL){
        protocolMutex = xSemaphoreCreateMutexStatic(&protocolMutexBuffer);
    }
    if(recvQueue == NULL){
        recvQueue = xQueueCreateStatic( queueSize, // The number of items the queue can hold.
                        maxLoraPacketSize,     // The size of each item in the queue
                        ucQueueStorage, // The buffer that will hold the items in the queue.
                        &xQueueBuffer );
    }
    if(protocolMutex == NULL){ ESP_LOGE(TAG, "protocol mutex cant be initialized!");};
    if(recvQueue == NULL){ ESP_LOGE(TAG, "recvQueue cant be initialized");};

    LoraDriverInit(config); //start driver
    protocolRunning = true;
    LoraStartRecv();        //begin listening
}

void protocolRestart(const RadioConfig config){
    
}

bool LoraProtocolRead(packetInfo& packet){
    if( recvQueue != NULL ) {
        while( xQueueReceive( recvQueue, &packet, portMAX_DELAY)  != pdPASS );
        return true;
    }else{
        ESP_LOGE(TAG, "Reading from unititialized queue. need to initialize protocol?");
        return false;
    }
}

//will attach the command in the next ACK bitmap we send back
bool LoraForwardCommand(uint8_t* command, uint8_t commandLength);

// --- TX Complete Callback ---
void protocolTXComplete() {
    ESP_LOGI(TAG, "TX Operation Complete (Interrupt Received)");
}

// --- Receive Callback ---
void protocolReceive(const uint8_t* rxBuffer, const size_t length) {
    ESP_LOGI(TAG, "Packet Received! Length: %u bytes", length);

    // Print raw bytes in Hex
    printf("RAW DATA: ");
    for (size_t i = 0; i < length; i++) {
        printf("%02X ", rxBuffer[i]);
    }
    printf("\n");

    // Optional: Try to print as ASCII if it looks like text
    // (Useful if you are sending "Hello World" style test messages)
    printf("ASCII:    ");
    for (size_t i = 0; i < length; i++) {
        char c = rxBuffer[i];
        if (c >= 32 && c <= 126) {
            printf("%c", c);
        } else {
            printf(".");
        }
    }
    printf("\n");
    LoraStartRecv();
}

// --- Crash Callback ---
void protocolCrash(const int16_t error, const char* msg) {
    ESP_LOGE(TAG, "CRITICAL DRIVER FAILURE!");
    ESP_LOGE(TAG, "Message: %s", msg);
    ESP_LOGE(TAG, "Error Code: %d", error);
    
    // In a test scenario, we might just want to hang here to inspect the logs
    // or trigger a simple restart after a delay.
    ESP_LOGW(TAG, "Restarting in 5 seconds...");
    vTaskDelay(pdMS_TO_TICKS(5000));
    esp_restart();
}
