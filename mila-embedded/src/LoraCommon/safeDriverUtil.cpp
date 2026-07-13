

#include <stdint.h>
#include <stddef.h>
#include "RadioLib.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "safeDriverUtil.hpp"
#include "../LoraCommon/Driver/Driver.hpp"
#include "LoraErrLog.hpp"
 
static const char* TAG = "safeDriverUtil";


result safeWaitForRecv(driverRecvPacket*& packet, const uint64_t timerExpireTime_us){
    if(getDriverInfo()->state == off) {
        logErr(TAG, driverOff);
        return Crashed;
    }

    LoraStartRecv();
    if(getDriverInfo()->state == off) {
        return Crashed;
    }

    driverInfo* info = waitForRecv(timerExpireTime_us);
    if(getDriverInfo()->state == off) {
        return Crashed;
    }

    if(info == NULL){
        logErr(TAG, RX_TIMEOUT);
        return Timeout;
    }
    if(!info->recvPacketReady){
        //unexpected interupt just triggered.
        ESP_LOGW(TAG, "unexpected interrupt triggered");
        return Unknown;
    }
    info->recvPacketReady = false;
    packet = &(info->recvPacket);
    return Success; 
}

result safeLoraTx(const driverSendPacket* packet, const uint64_t timerExpireTime_us){
    if(getDriverInfo()->state == off) {
        logErr(TAG, driverOff);
        return Crashed;
    }

    //queue transmission
    int16_t state = LoraTransmit(packet, timerExpireTime_us);
    ESP_LOGI(TAG, "LoraTransmit returned with state: %d", state);
    if(state == RADIOLIB_LORA_DETECTED){
        enterStandBy(); //stop driver. want clean after timeout.
        logErr(TAG, TX_Queue_Msg_Timeout);
        return Timeout;
    }
    if(state != RADIOLIB_ERR_NONE){
        logErr(TAG, state);
        return Crashed;
    }

    //wait for transmission to complete
    const int numPacketTimes = 2;
    if (waitForTXDone(numPacketTimes)){
        return Success;
    }
    ESP_LOGI(TAG, "wait for tx failed");
    if(getDriverInfo()->state == off) {
        return Crashed;
    }

    enterStandBy(); //stop driver. want clean after timeout.
    logErr(TAG, TX_Completion_Timeout);
    return Timeout;
}
