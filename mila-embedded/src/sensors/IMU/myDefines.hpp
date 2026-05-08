#ifndef IMU_DATA_H
#define IMU_DATA_H
//defines constants specific to IMU#include "../common/sensorHelper.hpp"
#include<stdint.h>
#define myId 9
#define numFrames 3
#define node_numData 12


#define node_numData 12
int32_t collect_IMU_temp_F(bool* cancelFrameSend);
int32_t collect_radiator_temp_F(bool* cancelFrameSend);
int32_t collect_humiditySense_temp_F(bool* cancelFrameSend);
int32_t collect_RH(bool* cancelFrameSend);
int32_t collect_posX_m(bool* cancelFrameSend);
int32_t collect_posY_m(bool* cancelFrameSend);
int32_t collect_posZ_m(bool* cancelFrameSend);
int32_t collect_accelY_mm_p_ss(bool* cancelFrameSend);
int32_t collect_accelZ_mm_p_ss(bool* cancelFrameSend);
int32_t collect_gyroX_deg_p_s(bool* cancelFrameSend);
int32_t collect_gryoY_deg_p_s(bool* cancelFrameSend);
int32_t collect_gyroZ_deg_p_s(bool* cancelFrameSend);

#define dataCollectorsList collect_IMU_temp_F, collect_radiator_temp_F, collect_humiditySense_temp_F, collect_RH, collect_posX_m, collect_posY_m, collect_posZ_m, collect_accelY_mm_p_ss, collect_accelZ_mm_p_ss, collect_gyroX_deg_p_s, collect_gryoY_deg_p_s, collect_gyroZ_deg_p_s

#endif