#ifndef precharge_DATA_H
#define precharge_DATA_H
//defines constants specific to precharge
#include <stdint.h>
#include <stdbool.h>
#define myId 3
#define numFrames 1
#define node_numData 5

int32_t collect_battery_V(bool* cancelFrameSend);
int32_t collect_motor_V(bool* cancelFrameSend);
int32_t collect_prechargeState(bool* cancelFrameSend);
int32_t collect_contactorState(bool* cancelFrameSend);
int32_t collect_prechargeLatched(bool* cancelFrameSend);

#define dataCollectorsList collect_battery_V, collect_motor_V, collect_prechargeState, collect_contactorState, collect_prechargeLatched

#endif