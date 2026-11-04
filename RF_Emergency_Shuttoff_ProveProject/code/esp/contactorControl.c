/* Proposed replacement for vitalsNode/contactorControl.c.
 * Keep the existing sequencing and CAN destinations. RF adds an inhibit and
 * a nonblocking off request so a full command queue cannot hide a fault. */
#include "contactorControl.h"
#include "vitalsData/vitalsData.h"
#include "pecan/pecan.h"
#include "rf/esp/RfCar.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdatomic.h>

typedef struct {
    vitalsContactorCommands command;
    unsigned generation;
} QueuedCommand;

static _Atomic(vitalsContactorState) state = allOff;
static atomic_bool offRequested = true;
static atomic_uint generation = 0;
static QueueHandle_t commands;
static const char *TAG = "contactorControl";

static void sendContactorCommand(bool enable, int node) {
    /* Check again at the last software point before sending an enable. */
    if (enable && !rfCarPermit()) return;
    CANPacket packet = {0};
    packet.id = combinedID(vitalsCommand,node);
    packet.dataSize = 1;
    packet.data[0] = enable ? enableContactor : disableContactor;
    if (!trySendPacket(&packet)) {
        /* The off state retries; an enable failure will time out the sequence. */
        ESP_LOGW(TAG,"CAN command not queued, node=%d enable=%d",node,enable);
    }
}

static void enter(vitalsContactorState next) {
    atomic_store(&state,next);
    if (next == allOff) {
        sendContactorCommand(false,prechargeID);
        sendContactorCommand(false,powerDistributionID);
    } else if (next == waitingForIntermoduleContactorEnable) {
        sendContactorCommand(true,powerDistributionID);
    } else if (next == waitingForPrechargeContactorEnable) {
        sendContactorCommand(true,prechargeID);
    }
}

void sendContactorControlCommand(vitalsContactorCommands command) {
    if (command == disableContactors) {
        atomic_fetch_add(&generation,1);
        atomic_store(&offRequested,true);
        return; /* Off never waits for queue space. */
    }
    if (!commands || !rfCarPermit()) return;
    QueuedCommand item = {command,atomic_load(&generation)};
    if (xQueueSend(commands,&item,0) != pdTRUE)
        ESP_LOGW(TAG,"Contactor command queue full; command dropped");
}

bool enableContactorsIfSafe(void) {
    /* Preserve the existing data-safety checks; RF permission is additional. */
    return commands && rfCarPermit() && requestEnableContactorsIfSafe();
}

void getContactorState(vitalsContactorState *result) {
    if (result) *result = atomic_load(&state);
}

static void controlTask(void *unused) {
    (void)unused;
    TickType_t entered = xTaskGetTickCount();
    TickType_t lastOff = entered;
    enter(allOff);
    for (;;) {
        bool forced = atomic_exchange(&offRequested,false);
        bool permit = rfCarPermit();
        TickType_t now = xTaskGetTickCount();
        vitalsContactorState current = atomic_load(&state);
        if (forced || !permit) {
            /* Drop old enables and completion messages from the old sequence. */
            xQueueReset(commands);
            if (forced || current != allOff || now-lastOff >= pdMS_TO_TICKS(20)) {
                enter(allOff); lastOff = now;
            }
            vTaskDelay(1);
            continue;
        }
        if ((current == waitingForIntermoduleContactorEnable ||
             current == waitingForPrechargeContactorEnable) &&
            now-entered >= pdMS_TO_TICKS(10000)) {
            sendContactorControlCommand(disableContactors);
            continue;
        }
        QueuedCommand item;
        if (xQueueReceive(commands,&item,1) != pdTRUE) {
            /* Reissue off while off, even when RF is healthy. */
            if (current == allOff && now-lastOff >= pdMS_TO_TICKS(20)) {
                enter(allOff); lastOff = now;
            }
            continue;
        }
        if (item.generation != atomic_load(&generation) ||
            atomic_load(&offRequested) || !rfCarPermit()) continue;
        vitalsContactorState next = current;
        if (current == allOff && item.command == enableContactors)
            next = waitingForIntermoduleContactorEnable;
        else if (current == waitingForIntermoduleContactorEnable &&
                 item.command == interModuleContactorsEnabled)
            next = waitingForPrechargeContactorEnable;
        else if (current == waitingForPrechargeContactorEnable &&
                 item.command == prechargeContactorsEnabled)
            next = allOn;
        if (next != current) { enter(next); entered = xTaskGetTickCount(); }
    }
}

void contactorControlInit(void) {
    if (commands) return;
    commands = xQueueCreate(10,sizeof(QueuedCommand));
    configASSERT(commands);
    configASSERT(xTaskCreate(controlTask,"contactorControl",4096,NULL,8,NULL)==pdPASS);
}
