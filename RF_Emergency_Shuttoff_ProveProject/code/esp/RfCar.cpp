#include "RfCar.h"
#include "RfRadio.h"
#include "RfReviewConfig.h"
#include "../rf/rf_nodes.h"
#include "vitalsNode/contactorControl.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static RfGuard guard;
static RfNodes nodes;
static bool initialized = false;
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static const int nodeIds[RF_NODE_COUNT] = {
    RF_LOGIC_NODE, RF_12V_NODE, RF_COMPUTE_NODE, RF_BACKUP_POWER_NODE
};
static uint64_t nowUs() { return (uint64_t)esp_timer_get_time(); }

static bool mappingReady() {
    if (!RF_PHYSICAL_SHUTDOWN_REVIEWED || RF_STATUS_FUNCTION < 9 ||
        RF_STATUS_FUNCTION > 15 || RF_REPLY_FUNCTION < 9 ||
        RF_REPLY_FUNCTION > 15 || RF_STATUS_FUNCTION == RF_REPLY_FUNCTION) return false;
    for (unsigned i=0; i<RF_NODE_COUNT; ++i) {
        if (nodeIds[i] < 0 || nodeIds[i] > 127 || nodeIds[i] == vitalsID) return false;
        for (unsigned j=0; j<i; ++j) if (nodeIds[i] == nodeIds[j]) return false;
    }
    return true;
}

extern "C" bool rfCarPermit() {
    portENTER_CRITICAL(&lock);
    /* Also evaluate current time here, so a delayed monitor cannot leave a
       stale permission visible to the contactor task. No I/O under this lock. */
    if (initialized) {
        rf_tick(&guard, nowUs());
        rf_nodes_ready(&guard, mappingReady() && rf_nodes_all_ready(&nodes, nowUs()));
    }
    bool permit = initialized && rf_permit(&guard);
    portEXIT_CRITICAL(&lock);
    return permit;
}

extern "C" bool rfCarLocalReset(bool deliberate, bool physicallyOff) {
    portENTER_CRITICAL(&lock);
    uint64_t now = nowUs();
    rf_nodes_ready(&guard, mappingReady() && rf_nodes_all_ready(&nodes, now));
    bool result = initialized && rf_reset(&guard, now, deliberate, physicallyOff);
    portEXIT_CRITICAL(&lock);
    return result;
}

static int16_t onNodeReply(CANPacket *p) {
    if (!mappingReady() || p->rtr || p->extendedID || p->dataSize != 8) return GEN_FAILURE;
    unsigned index;
    for (index=0; index<RF_NODE_COUNT; ++index)
        if ((int)getNodeId((uint32_t)p->id) == nodeIds[index]) break;
    portENTER_CRITICAL(&lock);
    bool valid = initialized && rf_nodes_reply(&nodes,index,p->data,nowUs());
    if (valid && p->data[1] == 0) rf_trip(&guard, RF_NODE_LOST);
    portEXIT_CRITICAL(&lock);
    return valid ? SUCCESS : GEN_FAILURE;
}

extern "C" void rfCarRegisterCan(PCANListenParamsCollection *listeners) {
    if (!mappingReady()) return; /* The unconfigured review build cannot arm. */
    CANListenParam entry = {};
    entry.listen_id = combinedID(RF_REPLY_FUNCTION, 0);
    entry.mt = MATCH_FUNCTION; entry.handler = onNodeReply;
    configASSERT(addParam(listeners, entry) == SUCCESS);
}

static void monitorTask(void *) {
    uint64_t lastOff = 0;
    bool previousPermit = false;
    for (;;) {
        uint64_t now = nowUs();
        uint8_t data[8];
        portENTER_CRITICAL(&lock);
        rf_tick(&guard, now);
        rf_nodes_ready(&guard, mappingReady() && rf_nodes_all_ready(&nodes, now));
        bool permit = rf_permit(&guard);
        if (previousPermit && !permit) nodes.sent_us = now - RF_CAN_PERIOD_US;
        bool publish = mappingReady() && rf_nodes_poll(&nodes, now, permit, data);
        portEXIT_CRITICAL(&lock);
        previousPermit = permit;

        /* This existing entry point must use the nonblocking off latch in the
           supplied patch. Never put the RF monitor behind the command queue. */
        if (!permit && (lastOff == 0 || now - lastOff >= 20000)) {
            sendContactorControlCommand(disableContactors);
            lastOff = now;
        }
        if (publish) {
            CANPacket p = {};
            p.id = combinedID(RF_STATUS_FUNCTION, vitalsID);
            p.dataSize = 8;
            for (unsigned i=0; i<8; ++i) p.data[i] = data[i];
            /* Added Pecan API: try once, no delay and no reboot on failure. */
            (void)trySendPacket(&p);
        }
        vTaskDelay(1); /* One RTOS tick, NOT a promise of one millisecond. */
    }
}

static void radioTask(void *) {
    if (!rfRadioInit()) {
        portENTER_CRITICAL(&lock); rf_trip(&guard, RF_RADIO_ERROR); portEXIT_CRITICAL(&lock);
        vTaskDelete(nullptr); return;
    }
    for (;;) {
        RfFrame request, reply;
        uint64_t started = nowUs();
        portENTER_CRITICAL(&lock);
        bool due = rf_make_request(&guard, started, &request);
        portEXIT_CRITICAL(&lock);
        if (due) {
            bool sent = rfRadioSend(request, started + 22000);
            if (sent) {
                while (rfRadioRead(reply, started + RF_REPLY_WINDOW_US)) {
                    portENTER_CRITICAL(&lock);
                    bool accepted = rf_receive(&guard, &reply, nowUs());
                    portEXIT_CRITICAL(&lock);
                    if (accepted) break;
                }
            }
            /* A missed transmission/response does not move last_good_us.
               The independent monitor still checks the 200 ms limit. */
        }
        vTaskDelay(1);
    }
}

extern "C" void rfCarInit() {
    uint64_t session = ((uint64_t)esp_random()<<32) | esp_random();
    /* Random boot nonce: replay separation, not authentication. A zero nonce
       intentionally inhibits operation. Never restore an old RfGuard image. */
    uint32_t epoch = (uint32_t)(session ^ (session>>32));
    portENTER_CRITICAL(&lock);
    rf_init(&guard,session,nowUs());
    rf_nodes_init(&nodes,epoch ? epoch : 1);
    initialized = true;
    portEXIT_CRITICAL(&lock);
    configASSERT(xTaskCreate(monitorTask,"rf_guard",4096,nullptr,9,nullptr)==pdPASS);
    configASSERT(xTaskCreate(radioTask,"rf_radio",6144,nullptr,8,nullptr)==pdPASS);
}
