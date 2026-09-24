#include <inttypes.h>
#include <stdint.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "../espBase/debug_esp.h"
#include "../pecan/pecan.h"
#include "../programConstants.h"

#define HB_ONLY_STACK_SIZE 4096
#define HB_PERIOD_MS 1000
#define HB_RESPONSE_WINDOW_MS 250

static const char* TAG = "VitalsHBOnly";

static StaticTask_t receiveTaskBuffer;
static StackType_t receiveTaskStack[HB_ONLY_STACK_SIZE];
static StaticTask_t heartbeatTaskBuffer;
static StackType_t heartbeatTaskStack[HB_ONLY_STACK_SIZE];

static volatile uint32_t heartbeatSequence;
static volatile uint32_t receivedHeartbeatSequence;
static volatile int64_t heartbeatSentAtUs;
static volatile int64_t heartbeatResponseTimeUs;

static int16_t receiveHeartbeat(CANPacket* packet) {
    const uint32_t responderId = getNodeId((uint32_t)packet->id);
    const int64_t responseTimeUs = esp_timer_get_time() - heartbeatSentAtUs;

    heartbeatResponseTimeUs = responseTimeUs;
    receivedHeartbeatSequence = heartbeatSequence;
    ESP_LOGI(TAG, "received HB pong from node %" PRIu32 " in %" PRIi64 " us", responderId, responseTimeUs);
    return SUCCESS;
}

static void receiveMessages(void* unused) {
    (void)unused;
    PCANListenParamsCollection listeners = {
        .arr = {{0}},
        .defaultHandler = defaultPacketRecv,
        .size = 0,
    };
    const CANListenParam heartbeatPong = {
        .listen_id = combinedID(HBPong, vitalsID),
        .handler = receiveHeartbeat,
        .mt = MATCH_FUNCTION,
    };

    if (addParam(&listeners, heartbeatPong) != SUCCESS) {
        ESP_LOGE(TAG, "no room for heartbeat response handler");
        vTaskDelete(NULL);
    }

    for (;;) { waitPackets(&listeners); }
}

static void sendAndCheckHeartbeats(void* unused) {
    (void)unused;
    for (;;) {
        CANPacket ping = {0};
        ping.id = combinedID(HBPing, vitalsID);
        setRTR(&ping);

        heartbeatSequence++;
        heartbeatSentAtUs = esp_timer_get_time();
        sendPacket(&ping);
        ESP_LOGI(TAG, "sent HB ping");

        vTaskDelay(pdMS_TO_TICKS(HB_RESPONSE_WINDOW_MS));
        if (receivedHeartbeatSequence == heartbeatSequence) {
            ESP_LOGI(TAG, "HB response received in %" PRIi64 " us", heartbeatResponseTimeUs);
        } else {
            ESP_LOGW(TAG, "no HB response within %d ms", HB_RESPONSE_WINDOW_MS);
        }

        vTaskDelay(pdMS_TO_TICKS(HB_PERIOD_MS - HB_RESPONSE_WINDOW_MS));
    }
}

void app_main(void) {
    base_ESP_init();
    const pecanInit config = {.nodeId = vitalsID, .pin1 = 17, .pin2 = 16};
    pecan_CanInit(config);

    if (xTaskCreateStaticPinnedToCore(receiveMessages, "hb_receive", HB_ONLY_STACK_SIZE, NULL,
                                      tskIDLE_PRIORITY + 1, receiveTaskStack, &receiveTaskBuffer,
                                      tskNO_AFFINITY) == NULL) {
        ESP_LOGE(TAG, "failed to create CAN receive task");
        return;
    }

    if (xTaskCreateStaticPinnedToCore(sendAndCheckHeartbeats, "hb_send", HB_ONLY_STACK_SIZE, NULL,
                                      tskIDLE_PRIORITY + 1, heartbeatTaskStack, &heartbeatTaskBuffer,
                                      tskNO_AFFINITY) == NULL) {
        ESP_LOGE(TAG, "failed to create heartbeat task");
    }
}
