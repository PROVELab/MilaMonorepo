#include <atomic>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"

#include <RadioLib.h>

#include "LoraErrLog.hpp"
#include "../LoraCommon/blastProtocolConfig.hpp"

static const char* TAG = "Lora_Err_Log";

//for logging regular errors over Lora
static Error_Type RadioLibErrBuffer[maxRadioErrs] = {0};  //will feed errors into front of next burst, if any errors available
static uint8_t RadioLibErrCount=0;

static std::atomic<custom_Err_Code_Flags> custom_err_flags;
static SemaphoreHandle_t radioLibErrMutex = NULL; //mutex to take or add to msg queue.
//

//may be called again on restart. Guaranteed not to have other Err functions running when called.
void initErr(){ 
    if (radioLibErrMutex != NULL){
        radioLibErrMutex = xSemaphoreCreateMutex(); //log and read errors internally 
    }
    RadioLibErrCount = 0;
    custom_err_flags.store(0);
 }


void logErr(int16_t err){
    if(err == RADIOLIB_ERR_NONE) return;
    ESP_LOGE(TAG, "Vitals/Lora error code %d being raised", err);
    if(err < 0){    //radioLibErr
        if(RadioLibErrCount == maxRadioErrs) return;    //already logging max num errors

        xSemaphoreTake(radioLibErrMutex, portMAX_DELAY);    //add the error
        for(int i=0;i<RadioLibErrCount; i++){
            if (err == RadioLibErrBuffer[i]) return;    //this error is already logged
        }
        RadioLibErrBuffer[RadioLibErrCount++] = err;
        xSemaphoreGive(radioLibErrMutex);
    }else{  //one of our custom errors (positive)
        if(err > maxErr_Code_Num){
            ESP_LOGI(TAG, "Chat ur cooked, attempted to log errCode %d, when maxErrCode is %d", err, maxErr_Code_Num);
            return;
        }
        custom_err_flags.fetch_or(1 << err);
    }
}

//populates errPacket with Error_Type errors.
//returns number of errors logged into errPacket
uint8_t generateErrPacket(int16_t* errPacket, uint8_t maxErrCount){
    while(xSemaphoreTake(radioLibErrMutex, portMAX_DELAY) != pdPASS);
    uint8_t errIndex = 0;

    custom_Err_Code_Flags errFlags = custom_err_flags.fetch_and(0);
    if(errFlags == 0 && RadioLibErrCount == 0){
        return 0;
    }

    //update their packet with errors
    Error_Type errCode = 1;    //the starting error code for custom error flags
    while( (errFlags != 0) && (errIndex < maxErrCount) ){
        if(errFlags & 1){
            errPacket[errIndex] = errCode;
            errIndex++;
        }
        errCode++;
    }
    uint8_t errorsToLog = errIndex + RadioLibErrCount;
    if(errorsToLog > maxErrCount) {errorsToLog = maxErrCount;}

    memcpy(errPacket + errIndex, RadioLibErrBuffer, errorsToLog * sizeof(Error_Type));
    RadioLibErrCount = 0;
    xSemaphoreGive(radioLibErrMutex);
}