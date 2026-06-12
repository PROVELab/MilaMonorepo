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

// ----- intermoduleCommand -----
typedef struct __attribute__((packed)) {
    int32_t prechargeCommands;
} intermoduleCommand_args_t;

void onintermoduleCommand(intermoduleCommand_args_t args);

int32_t collect_LV_Battery_mV(bool* cancelFrameSend);
int32_t collect_LV_Battery_mA(bool* cancelFrameSend);
int32_t collect_CoolantPumpAvgCurrent_mA(bool* cancelFrameSend);
int32_t collect_CoolantPumpPeakCurrent_mA(bool* cancelFrameSend);
int32_t collect_CoolantPump_Freq_kHz(bool* cancelFrameSend);
int32_t collect_CoolantPumpDutyCycle(bool* cancelFrameSend);
int32_t collect_CoolantDriver_Fault(bool* cancelFrameSend);

#define SENSOR_MAX_RECV_DATA_FIELDS 1
#define SENSOR_RECV_MASK_BITS 0

#define dataCollectorsList collect_LV_Battery_mV, collect_LV_Battery_mA, collect_CoolantPumpAvgCurrent_mA, collect_CoolantPumpPeakCurrent_mA, collect_CoolantPump_Freq_kHz, collect_CoolantPumpDutyCycle, collect_CoolantDriver_Fault

#endif