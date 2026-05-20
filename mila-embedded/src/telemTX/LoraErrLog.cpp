#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"


#include <RadioLib.h>

#include "../LoraCommon/LoraErrLog.hpp"
#include "../LoraCommon/blastProtocolConfig.hpp"
#include "TXBlastProtocolHelper.hpp"    //for queue access

//for logging regular errors over Lora


#define errorDuplicateCheckWindow 8 //wont log an error more than twice if present in the last 8 errors, after logging 8 errors
static Error_Type ErrBuffer[maxErrorCount] = {0}; //stores raw error codes from RadioLib. will be sent over Lora with custom error flags
static uint8_t ErrCount = 0; //number of RadioLib errors currently stored

static SemaphoreHandle_t ErrMutex = NULL; //mutex to take or add to msg queue.
static StaticSemaphore_t errMutexBuffer;
//

//may be called again on restart. Guaranteed not to have other Err functions running when called.
void initErr(){ 
    if (ErrMutex == NULL){
        ErrMutex = xSemaphoreCreateMutexStatic(&errMutexBuffer);
    }
    xSemaphoreTake(ErrMutex, portMAX_DELAY);
    ErrCount = 0;
    xSemaphoreGive(ErrMutex);
 }


void logErr(const char* TAG, int16_t err){
    if(err == RADIOLIB_ERR_NONE) return;
    ESP_LOGE(TAG, "Vitals/Lora error code %d being raised", err);
    xSemaphoreTake(ErrMutex, portMAX_DELAY);

    if(ErrCount == maxErrorCount){
        ESP_LOGW(TAG, "RadioLib error buffer full, cannot log more errors for this burst");
        xSemaphoreGive(ErrMutex);
        return;
    }

    if(ErrCount > errorDuplicateCheckWindow){
        for(int i = ErrCount - errorDuplicateCheckWindow; i < ErrCount; i++){
            if(ErrBuffer[i] == err){
                ESP_LOGW(TAG, "Error code %d already recently in buffer, not logging again", err);
                xSemaphoreGive(ErrMutex);
                return;
            }
        }
    }
    ErrBuffer[ErrCount] = err;
    ErrCount++;
    xSemaphoreGive(ErrMutex);
}


uint8_t getErrorPacket(int16_t* errPacket){
    xSemaphoreTake(ErrMutex, portMAX_DELAY);
    if (ErrCount > 0) {
        memcpy(errPacket, ErrBuffer, ErrCount * sizeof(Error_Type));
    }
    uint8_t tempErrCount = ErrCount; 
    ErrCount = 0;   //reset the size
    xSemaphoreGive(ErrMutex);
    return tempErrCount;
}
