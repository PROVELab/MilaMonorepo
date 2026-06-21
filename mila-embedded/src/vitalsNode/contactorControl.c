#include "vitalsGen/vitalsPacketRecvLUT.h"
#include "esp_log.h"
#include "pecan/pecan.h"
#include "../programConstants.h"
#include <stdbool.h>
#include <stdatomic.h>
#include <stdint.h>

#include "contactorControl.h"
#include "vitalsGen/vitalsStructs.h"
#include "vitalsData/vitalsData.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char* TAG = "VitalscontactorControl";

#define CONTACTOR_CONTROL_STACK_SIZE 4092
#define DISABLE_CONTACTOR 0
#define ENABLE_CONTACTOR 1
#define CONTACTOR_WAIT_TICKS pdMS_TO_TICKS(10000)
#define COMMAND_FLAG(command) (1u << (uint32_t)(command))

typedef void (*ContactorStateEntryAction)(void);

typedef struct {
    uint32_t successMask;
    TickType_t timeToWaitForSuccess;
    ContactorStateEntryAction onEntry;
} ContactorStateConfig;

static _Atomic(vitalsContactorState) precharge_state = allOff;
static StaticQueue_t contactorCommandQueueBuffer;
static uint8_t contactorCommandQueueStorage[10 * sizeof(vitalsContactorCommands)];
static QueueHandle_t contactorCommandQueue = NULL;

static StaticTask_t contactorControlTaskBuffer;
static StackType_t contactorControlTaskStack[CONTACTOR_CONTROL_STACK_SIZE];

static void contactorControlTask(void* pvParameters);
static void sendContactorCommand(int8_t enableContactor, int nodeID);
static void enterAllOff(void);
static void enterWaitingForIntermodule(void);
static void enterWaitingForPrecharge(void);
static void enterAllOn(void);
static vitalsContactorState getCurrentState(void);
static void transitionToState(vitalsContactorState nextState, TickType_t* ticksToWait);
static void advanceState(TickType_t* ticksToWait);

static const ContactorStateConfig stateConditionsTable[] = {
    [allOff] = {
        .successMask = COMMAND_FLAG(enableContactors),
        .timeToWaitForSuccess = portMAX_DELAY,
        .onEntry = enterAllOff,
    },
    [waitingForIntermoduleContactorEnable] = {
        .successMask = COMMAND_FLAG(interModuleContactorsEnabled),
        .timeToWaitForSuccess = CONTACTOR_WAIT_TICKS,
        .onEntry = enterWaitingForIntermodule,
    },
    [waitingForPrechargeContactorEnable] = {
        .successMask = COMMAND_FLAG(prechargeContactorsEnabled),
        .timeToWaitForSuccess = CONTACTOR_WAIT_TICKS,
        .onEntry = enterWaitingForPrecharge,
    },
    [allOn] = {
        .successMask = 0,
        .timeToWaitForSuccess = portMAX_DELAY,
        .onEntry = enterAllOn,
    },
};

void contactorControlInit(void){
    if(contactorCommandQueue != NULL){
        return;
    }

    contactorCommandQueue = xQueueCreateStatic(10, sizeof(vitalsContactorCommands), contactorCommandQueueStorage,
                                               &contactorCommandQueueBuffer);
    configASSERT(contactorCommandQueue != NULL);

    (void)xTaskCreateStatic(contactorControlTask, "contactorControl", CONTACTOR_CONTROL_STACK_SIZE, NULL, 5,
                            contactorControlTaskStack, &contactorControlTaskBuffer);
}

void sendContactorControlCommand(vitalsContactorCommands command){

    if(contactorCommandQueue != NULL){
        (void)xQueueSend(contactorCommandQueue, &command, portMAX_DELAY);
    }
}

bool enableContactorsIfSafe(){
    if(contactorCommandQueue == NULL){
        return false;
    }

    if(!requestEnableContactorsIfSafe()){
        ESP_LOGI(TAG, "Failed to queue safe contactor enable request.");
        return false;
    }

    return true;
}

void getContactorState(vitalsContactorState* state){
    if(state != NULL){
        *state = atomic_load_explicit(&precharge_state, memory_order_seq_cst);
    }
}

static void contactorControlTask(void* pvParameters){
    (void)pvParameters;

    const vitalsContactorState initialState = getCurrentState();
    TickType_t ticksToWait = stateConditionsTable[initialState].timeToWaitForSuccess;
    stateConditionsTable[initialState].onEntry();

    for(;;){
        const vitalsContactorState currentState = getCurrentState();
        vitalsContactorCommands command;
        const TickType_t recvStartTime = xTaskGetTickCount();
        if(xQueueReceive(contactorCommandQueue, &command, ticksToWait) != pdTRUE){
            ESP_LOGW(TAG, "Timed out waiting for contactor sequence completion in state %d", (int)currentState);
            transitionToState(allOff, &ticksToWait);
            continue;
        }

        if(command == disableContactors){
            transitionToState(allOff, &ticksToWait);
            continue;
        }

        if(ticksToWait != portMAX_DELAY){
            const TickType_t ticksElapsed = xTaskGetTickCount() - recvStartTime;
            ticksToWait = ticksElapsed >= ticksToWait ? 0 : ticksToWait - ticksElapsed;
        }

        const ContactorStateConfig* stateConfig = &stateConditionsTable[currentState];
        if((stateConfig->successMask & COMMAND_FLAG(command)) != 0u){
            advanceState(&ticksToWait);
            continue;
        }

        ESP_LOGI(TAG, "ignoring command %d. current state is %d", (int)command, (int)currentState);
    }
}

static void enterAllOff(void){
    sendContactorCommand(DISABLE_CONTACTOR, prechargeID);
    sendContactorCommand(DISABLE_CONTACTOR, powerDistributionID);
}

static void enterWaitingForIntermodule(void){
    sendContactorCommand(ENABLE_CONTACTOR, powerDistributionID);
}

static void enterWaitingForPrecharge(void){
    sendContactorCommand(ENABLE_CONTACTOR, prechargeID);
}

static void enterAllOn(void){
}

static vitalsContactorState getCurrentState(void){
    return atomic_load_explicit(&precharge_state, memory_order_seq_cst);
}

static void transitionToState(vitalsContactorState nextState, TickType_t* ticksToWait){
    atomic_store_explicit(&precharge_state, nextState, memory_order_seq_cst);
    stateConditionsTable[nextState].onEntry();
    *ticksToWait = stateConditionsTable[nextState].timeToWaitForSuccess;
}

static void advanceState(TickType_t* ticksToWait){
    transitionToState((vitalsContactorState)(getCurrentState() + 1), ticksToWait);
}

static void sendContactorCommand(int8_t enable, int nodeID){
    int8_t payload = enable ? enableContactor : disableContactor;

    if(nodeID != prechargeID && nodeID != powerDistributionID){
        ESP_LOGW(TAG, "warning, sending contactor command to unknown node. what are you doing?");
    }

    CANPacket p = {0};

    p.id = combinedID(vitalsCommand, nodeID);
    if(writeData(&p, &payload, 1) != SUCCESS){
        ESP_LOGE(TAG, "Failed to write contactor command payload: %d", (int)payload);
        return;
    }

    sendPacket(&p);
    ESP_LOGI(TAG, "Sent contactor command %d to node %d", (int)payload, nodeID);
}
