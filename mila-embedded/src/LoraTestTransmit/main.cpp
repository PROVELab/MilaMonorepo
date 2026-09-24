#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "esp_timer.h"

#include <cstring>
#include <inttypes.h>

#include "../LoraCommon/Driver/Driver.hpp"
#include "../LoraCommon/LoraErrLog.hpp"
#include "../LoraCommon/safeDriverUtil.hpp"

static const char* TAG = "main";

#define timeout_ms 2000
#define timeout_us (timeout_ms * 1000)
#define seqNum 9900

static uint64_t counter = 1;

// The simple-test build does not link either telemetry protocol's error sink.
void logErr(const char* tag, int16_t error) {
    ESP_LOGE(tag, "LoRa test error: %d", error);
}

void protocolTXComplete() {}
void protocolRecv(const driverRecvPacket* packet) {
    ESP_LOGW(TAG, "Unexpected receive of %zu bytes while transmitting", packet->dataSize);
}
void protocolCrash(const int16_t error, const char* msg) {
    ESP_LOGE(TAG, "protocolCrash called from %s with error %d", msg, error);
}

extern "C" void app_main(void) {
    vTaskDelay(pdMS_TO_TICKS(2000));
    ESP_LOGI(TAG, "Starting LoRa TX test");

    RadioConfig cfg = getStandardConfig(BoardType::Ebyte_SX1262, TestMode::lowPower);
    LoraDriverInit(&cfg);

    constexpr size_t loopSize = 30;
    for (;;) {
        uint8_t payload[sizeof(uint16_t) + sizeof(counter) * loopSize];
        const uint16_t sequence = seqNum;
        memcpy(payload, &sequence, sizeof(sequence));
        for (size_t i = 0; i < loopSize; ++i) {
            memcpy(payload + sizeof(sequence) + sizeof(counter) * i, &counter, sizeof(counter));
        }

        const driverSendPacket msg{sizeof(payload), payload};
        const result txResult = safeLoraTx(&msg, esp_timer_get_time() + timeout_us);
        if (txResult == Success) {
            ESP_LOGI(TAG, "TX_DONE for counter %" PRIu64, counter);
            ++counter;
        } else {
            ESP_LOGW(TAG, "Transmit of counter %" PRIu64 " failed: result %d", counter, txResult);
            if (txResult == Crashed) {
                break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
