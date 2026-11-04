#include "RfRadio.h"
#include "RfReviewConfig.h"
#include "LoraCommon/Driver/Driver.hpp"
#include "RadioLib.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Only the RF radio task may call this adapter. Do NOT also run runProtocol().
 * The car watchdog is a different task and does not wait on this driver. */
bool rfRadioInit() {
    RadioConfig cfg = getStandardConfig(Ebyte_SX1262, lowPower);
    cfg.SpreadingFactor = RF_REVIEW_SF;
    cfg.codingRate = RF_REVIEW_CR;
    LoraDriverInit(&cfg);
    LoraStartRecv();
    return getDriverInfo()->state != off;
}

bool rfRadioSend(const RfFrame &f, uint64_t deadline) {
    uint8_t bytes[RF_FRAME_SIZE]; rf_encode(&f, bytes);
    driverSendPacket packet = {sizeof(bytes), bytes};
    if (!LoraRfPrepareTx()) return false;
    /* LoraTransmit copies into the chip before returning, not into an app queue. */
    if (LoraTransmit(&packet, deadline) != RADIOLIB_ERR_NONE) return false;
    while ((uint64_t)esp_timer_get_time() < deadline) {
        driverInfo *info = waitForDriverAction(1000);
        if (info && info->state == off) return false;
        if (info && (info->recvPacket.irqFlags & RADIOLIB_SX126X_IRQ_TX_DONE)) {
            LoraStartRecv();
            return (uint64_t)esp_timer_get_time() <= deadline;
        }
        vTaskDelay(1);
    }
    enterStandBy(); LoraStartRecv();
    return false;
}

bool rfRadioRead(RfFrame &frame, uint64_t deadline) {
    while ((uint64_t)esp_timer_get_time() < deadline) {
        driverRecvPacket copy;
        /* This added driver helper copies under its mutex and clears readiness.
         * Do not read the driver's mutable receive buffer from two tasks. */
        if (LoraTakeReceived(&copy)) {
            uint64_t now = (uint64_t)esp_timer_get_time();
            if (now > deadline || now - copy.receivedAt_us > 10000) continue;
            if (rf_decode(copy.data, copy.dataSize, &frame)) return true;
        }
        if (getDriverInfo()->state == off) return false;
        vTaskDelay(1);
    }
    return false;
}
