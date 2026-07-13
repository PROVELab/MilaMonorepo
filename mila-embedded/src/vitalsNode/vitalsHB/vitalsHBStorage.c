#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "../../programConstants.h"

#include "HBHelper.h"

static SemaphoreHandle_t HBMutex = NULL;
static StaticSemaphore_t HBMutexBuffer;

static int32_t HBMask = 0;
static int16_t HBTimes[numberOfNodes] = {0};


void initStorage(void){
    HBMutex = xSemaphoreCreateMutexStatic(&HBMutexBuffer);
}

void HB_Value_Update(int HBIndex, int16_t HBTime){
    if (HBMutex == NULL) return;
    xSemaphoreTake(HBMutex, portMAX_DELAY);
    HBTimes[HBIndex] = HBTime;
    HBMask |= 1 << HBIndex;
    xSemaphoreGive(HBMutex);
}

void get_and_clear_HB_Values(int32_t* HBMaskRetriever, int16_t* HBTimesRetriever){
    if (HBMutex == NULL) {
        memset(HBTimesRetriever, 0, sizeof(HBTimes));
        *HBMaskRetriever = 0;
        return;
    }
    xSemaphoreTake(HBMutex, portMAX_DELAY);
    memcpy(HBTimesRetriever, HBTimes, sizeof(HBTimes));
    memset(HBTimes, 0, sizeof(HBTimes));
    *HBMaskRetriever = HBMask;
    HBMask = 0;
    xSemaphoreGive(HBMutex);
}
