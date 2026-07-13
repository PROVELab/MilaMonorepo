#ifndef powerDistribution_DATA_H
#define powerDistribution_DATA_H
//defines constants specific to powerDistribution
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h> // For size_t
#define myId 10
#define numFrames 2
#define node_numData 7

#define SENSOR_HAS_COMMANDS

// ----- setCoolantDutyCycle -----
typedef struct __attribute__((packed)) {
    int32_t dutyCycle;
} setCoolantDutyCycle_args_t;

void onsetCoolantDutyCycle(setCoolantDutyCycle_args_t args);

// ----- setCoolantFrequency_HZ -----
typedef struct __attribute__((packed)) {
    int32_t frequency_HZ;
} setCoolantFrequency_HZ_args_t;

void onsetCoolantFrequency_HZ(setCoolantFrequency_HZ_args_t args);

int32_t collect_LV_Battery_mV(bool* cancelFrameSend);
int32_t collect_LV_Battery_mA(bool* cancelFrameSend);
int32_t collect_CoolantAvgCurrent_mA(bool* cancelFrameSend);
int32_t collect_CoolantPeakCurrent_mA(bool* cancelFrameSend);
int32_t collect_Coolant_Freq_kHz(bool* cancelFrameSend);
int32_t collect_CoolantDutyCycle(bool* cancelFrameSend);
int32_t collect_CoolantDriver_Fault(bool* cancelFrameSend);

#define SENSOR_MAX_RECV_DATA_FIELDS 1
#define SENSOR_RECV_MASK_BITS 1

#define dataCollectorsList collect_LV_Battery_mV, collect_LV_Battery_mA, collect_CoolantAvgCurrent_mA, collect_CoolantPeakCurrent_mA, collect_Coolant_Freq_kHz, collect_CoolantDutyCycle, collect_CoolantDriver_Fault

#endif