#include "driver/twai.h"
#include "esp_log.h"

#include <inttypes.h>
#include <limits.h>
#include <stdint.h>

#include "../../programConstants.h"
#include "../vitalsGen/vitalsPacketSendLUT.h"
#include "../vitalsHelper/vitalsHelper.h"
#include "HBHelper.h"
#include "../contactorControl.h"

static const char* TAG = "HBDataSend";

static inline void updateSlowestIndex(int32_t* slowestNodesArray, int16_t* worstTimesArray, uint8_t nodeIndex, int16_t time);
typedef struct {
    int32_t txErrCnt;
    int32_t rxErrCnt;
    int32_t busErrDelta;
    int32_t failedTxDelta;
    int32_t rxOverrunDelta;
    int32_t rxMissedDelta;
    int32_t rxQueueCount;
    int32_t twaiState;
} BusStatusSnapshot;
static BusStatusSnapshot collectBusStatusFields(void);
static inline int32_t deltaSinceLast(uint32_t current, uint32_t* previous);

void format_and_send_HB_info(int32_t HBMask, const int16_t* HBTimes) {
    int32_t slowestNodeIndices[slowestNodeCount] = {0};
    int16_t worstTimes[slowestNodeCount] = {0};
    vitalsContactorState contactorState = allOff;

    for (int i = 0; i < numberOfNodes; i++) {
        if (HBMask & (1 << i)) {
            updateSlowestIndex(slowestNodeIndices, worstTimes, (uint8_t)i, HBTimes[i]);
        }
    }

    getContactorState(&contactorState);
    const BusStatusSnapshot busStatus = collectBusStatusFields();

    sendVitalsUpdateArgs vitals_update_args = {
        .mask = 0,
        .vitalsContactorState = { .e = contactorState },
        .contactorStateLatched = 1,
        .TWAI_TX_Err_Cnt = busStatus.txErrCnt,
        .TWAI_RX_Err_Cnt = busStatus.rxErrCnt,
        .TWAI_Err_Cnt = busStatus.busErrDelta,
        .failed_TX_Cnt = busStatus.failedTxDelta,
        .RX_Overrun_Cnt = busStatus.rxOverrunDelta,
        .RX_Missed_Cnt = busStatus.rxMissedDelta,
        .RX_Recv_Queue_Cnt = busStatus.rxQueueCount,
        .slowestNode1_ID = (worstTimes[0] > 0) ? (int32_t)vitalsIndexToID((uint32_t)slowestNodeIndices[0]) : 0,
        .slowestNode1_time = worstTimes[0],
        .slowestNode2_ID = (worstTimes[1] > 0) ? (int32_t)vitalsIndexToID((uint32_t)slowestNodeIndices[1]) : 0,
        .slowestNode2_time = worstTimes[1],
        .slowestNode3_ID = (worstTimes[2] > 0) ? (int32_t)vitalsIndexToID((uint32_t)slowestNodeIndices[2]) : 0,
        .slowestNode3_time = worstTimes[2],
        .TWAI_STATE = { .i32 = busStatus.twaiState },
        .HBMask = HBMask,
    };

    ESP_LOGI(TAG, "Sending VitalsUpdate: TWAI_STATE=%" PRIi32 " HBMask=%" PRIi32
                  " slow1=(%" PRIi32 ",%" PRIi32 ") slow2=(%" PRIi32 ",%" PRIi32 ") slow3=(%" PRIi32 ",%" PRIi32 ")",
             vitals_update_args.TWAI_STATE.i32, vitals_update_args.HBMask,
             vitals_update_args.slowestNode1_ID, vitals_update_args.slowestNode1_time,
             vitals_update_args.slowestNode2_ID, vitals_update_args.slowestNode2_time,
             vitals_update_args.slowestNode3_ID, vitals_update_args.slowestNode3_time);
    ESP_LOGI(TAG, "VitalsUpdate args: contactor=%" PRIi32 " latched=%" PRIi32
                  " txErr=%" PRIi32 " rxErr=%" PRIi32 " busErr=%" PRIi32 " txFail=%" PRIi32
                  " rxOverrun=%" PRIi32 " rxMissed=%" PRIi32 " rxQueue=%" PRIi32
                  " slow1=(%" PRIi32 ",%" PRIi32 ") slow2=(%" PRIi32 ",%" PRIi32 ") slow3=(%" PRIi32 ",%" PRIi32 ")"
                  " twaiState=%" PRIi32 " hbMask=%" PRIi32,
             vitals_update_args.vitalsContactorState.i32, vitals_update_args.contactorStateLatched,
             vitals_update_args.TWAI_TX_Err_Cnt, vitals_update_args.TWAI_RX_Err_Cnt,
             vitals_update_args.TWAI_Err_Cnt, vitals_update_args.failed_TX_Cnt,
             vitals_update_args.RX_Overrun_Cnt, vitals_update_args.RX_Missed_Cnt,
             vitals_update_args.RX_Recv_Queue_Cnt, vitals_update_args.slowestNode1_ID,
             vitals_update_args.slowestNode1_time, vitals_update_args.slowestNode2_ID,
             vitals_update_args.slowestNode2_time, vitals_update_args.slowestNode3_ID,
             vitals_update_args.slowestNode3_time, vitals_update_args.TWAI_STATE.i32,
             vitals_update_args.HBMask);
    sendVitalsUpdateFunction(vitals_update_args);
}

static inline void updateSlowestIndex(int32_t* slowestNodesArray, int16_t* worstTimesArray, uint8_t nodeIndex, int16_t time) {
    for (int i = 0; i < slowestNodeCount; i++) {
        if (time > worstTimesArray[i]) {
            for (int j = slowestNodeCount - 1; j > i; j--) {
                worstTimesArray[j] = worstTimesArray[j - 1];
                slowestNodesArray[j] = slowestNodesArray[j - 1];
            }
            worstTimesArray[i] = time;
            slowestNodesArray[i] = nodeIndex;
            return;
        }
    }
}

static BusStatusSnapshot collectBusStatusFields(void) {
    BusStatusSnapshot snapshot = {
        .txErrCnt = 0,
        .rxErrCnt = 0,
        .busErrDelta = 0,
        .failedTxDelta = 0,
        .rxOverrunDelta = 0,
        .rxMissedDelta = 0,
        .rxQueueCount = 0,
        .twaiState = TWAI_PECAN_STOPPED,
    };

    twai_status_info_t status_info;
    const esp_err_t status_err = twai_get_status_info(&status_info);
    if (status_err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read TWAI bus status for VitalsUpdate (err=%d)", (int)status_err);
        return snapshot;
    }

    ESP_LOGI(TAG, "TWAI status snapshot: state=%d txq=%" PRIu32 " rxq=%" PRIu32 " tx_err=%" PRIu32
                  " rx_err=%" PRIu32 " tx_fail=%" PRIu32 " rx_miss=%" PRIu32 " rx_overrun=%" PRIu32
                  " arb_lost=%" PRIu32 " bus_err=%" PRIu32,
             (int)status_info.state, status_info.msgs_to_tx, status_info.msgs_to_rx,
             status_info.tx_error_counter, status_info.rx_error_counter, status_info.tx_failed_count,
             status_info.rx_missed_count, status_info.rx_overrun_count, status_info.arb_lost_count,
             status_info.bus_error_count);

    ESP_LOGD(TAG, "Bus Status - TXQ:%" PRIu32 ", RXQ:%" PRIu32 ", TX_err:%" PRIu32 ", RX_err:%" PRIu32 ", TX_fail:%" PRIu32 ", RX_miss:%" PRIu32 ", "
                  "RX_overrun:%" PRIu32 ", ARB_lost:%" PRIu32 ", BUS_err:%" PRIu32,
             status_info.msgs_to_tx, status_info.msgs_to_rx, status_info.tx_error_counter,
             status_info.rx_error_counter, status_info.tx_failed_count, status_info.rx_missed_count,
             status_info.rx_overrun_count, status_info.arb_lost_count, status_info.bus_error_count);

    static uint32_t errCnt = 0, txFails = 0, rxOverrun = 0, rxMissed = 0; // records previous value

    snapshot.txErrCnt = (int32_t)status_info.tx_error_counter;
    snapshot.rxErrCnt = (int32_t)status_info.rx_error_counter;
    snapshot.busErrDelta = deltaSinceLast(status_info.bus_error_count, &errCnt);
    snapshot.failedTxDelta = deltaSinceLast(status_info.tx_failed_count, &txFails);
    snapshot.rxOverrunDelta = deltaSinceLast(status_info.rx_overrun_count, &rxOverrun);
    snapshot.rxMissedDelta = deltaSinceLast(status_info.rx_missed_count, &rxMissed);
    snapshot.rxQueueCount = (int32_t)status_info.msgs_to_rx;
    snapshot.twaiState = (int32_t)status_info.state;
    return snapshot;
}

static inline int32_t deltaSinceLast(uint32_t current, uint32_t* previous) {
    uint32_t delta = (current >= *previous) ? (current - *previous) : current;
    *previous = current;
    return (delta > (uint32_t)INT32_MAX) ? INT32_MAX : (int32_t)delta;
}
