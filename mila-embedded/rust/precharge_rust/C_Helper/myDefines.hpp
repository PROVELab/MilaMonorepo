#ifndef precharge_DATA_H
#define precharge_DATA_H
//defines constants specific to precharge
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h> // For size_t
#define myId 3
#define numFrames 2
#define node_numData 9

#define SENSOR_HAS_COMMANDS

#ifdef __cplusplus
extern "C" {
#endif

// ----- setChargeCondition -----
typedef struct __attribute__((packed)) {
    int32_t min_MC_Voltage;
    int32_t minPercentCharged;
} setChargeCondition_args_t;

void onsetChargeCondition(setChargeCondition_args_t args);

int32_t collect_battery_V(bool* cancelFrameSend);
int32_t collect_motor_V(bool* cancelFrameSend);
int32_t collect_prechargeState(bool* cancelFrameSend);
int32_t collect_contactorState(bool* cancelFrameSend);
int32_t collect_IMD_frequency_Hz(bool* cancelFrameSend);
int32_t collect_IMD_Duty_Cycle_Percent(bool* cancelFrameSend);
int32_t collect_IMD_Hardware_Fault_Indicator(bool* cancelFrameSend);
int32_t collect_min_MC_Voltage(bool* cancelFrameSend);
int32_t collect_minPercentCharged(bool* cancelFrameSend);

#ifdef __cplusplus
}
#endif

#define SENSOR_MAX_RECV_DATA_FIELDS 2
#define SENSOR_RECV_MASK_BITS 0

#define dataCollectorsList collect_battery_V, collect_motor_V, collect_prechargeState, collect_contactorState, collect_IMD_frequency_Hz, collect_IMD_Duty_Cycle_Percent, collect_IMD_Hardware_Fault_Indicator, collect_min_MC_Voltage, collect_minPercentCharged

#endif
