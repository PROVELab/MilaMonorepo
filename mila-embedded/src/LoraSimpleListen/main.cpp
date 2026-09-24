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

static uint64_t firstNum = 0;
static uint64_t lastNum = 0;
static uint64_t recvCount = 0;
static bool haveFirstNum = false;
static bool haveLastNum = false;

// The simple-test build does not link either telemetry protocol's error sink.
void logErr(const char* tag, int16_t error) {
    ESP_LOGE(tag, "LoRa test error: %d", error);
}

void protocolTXComplete() {}
void protocolRecv(const driverRecvPacket*) {}
void protocolCrash(const int16_t error, const char* msg) {
    ESP_LOGE(TAG, "protocolCrash called from %s with error %d", msg, error);
}

static void printPacket(const driverRecvPacket& packet) {
    constexpr size_t loopSize = 30;
    constexpr size_t expectedSize = sizeof(uint16_t) + loopSize * sizeof(uint64_t);
    if (packet.dataSize != expectedSize) {
        ESP_LOGW(TAG, "Unexpected packet size: %zu (expected %zu)", packet.dataSize, expectedSize);
        return;
    }

    uint16_t recvSeqNum;
    memcpy(&recvSeqNum, packet.data, sizeof(recvSeqNum));
    if (recvSeqNum != seqNum) {
        ESP_LOGW(TAG, "Unexpected sequence number: %u", recvSeqNum);
        return;
    }

    uint64_t value;
    memcpy(&value, packet.data + sizeof(recvSeqNum), sizeof(value));
    for (size_t i = 1; i < loopSize; ++i) {
        uint64_t nextValue;
        memcpy(&nextValue, packet.data + sizeof(recvSeqNum) + i * sizeof(nextValue), sizeof(nextValue));
        if (nextValue != value) {
            ESP_LOGW(TAG, "Inconsistent payload data");
            return;
        }
    }

    if (!haveFirstNum) {
        firstNum = value;
        haveFirstNum = true;
    }
    if (haveLastNum && value <= lastNum) {
        ESP_LOGW(TAG, "Packet counter is out of order");
    }
    lastNum = value;
    haveLastNum = true;
    ++recvCount;
    printf("recv val: %" PRIu64 ". Ratio: (%" PRIu64 "/%" PRIu64 ")\n",
           value, recvCount, value - firstNum + 1);
}

extern "C" void app_main(void) {
    vTaskDelay(pdMS_TO_TICKS(2000));
    ESP_LOGI(TAG, "Starting LoRa RX test");

    RadioConfig cfg = getStandardConfig(BoardType::Ebyte_SX1262, TestMode::highPower);
    LoraDriverInit(&cfg);

    for (;;) {
        driverRecvPacket* packet = nullptr;
        const result rxResult = safeWaitForRecv(packet, esp_timer_get_time() + timeout_us);
        if (rxResult == Success) {
            printPacket(*packet);
        } else if (rxResult == Timeout) {
            ESP_LOGI(TAG, "No packet received in %d ms", timeout_ms);
        } else {
            ESP_LOGE(TAG, "Receive failed: result %d", rxResult);
            if (rxResult == Crashed) {
                break;
            }
        }
    }
}
