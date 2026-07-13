#ifndef IMU_DATA_H
#define IMU_DATA_H
//defines constants specific to IMU
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h> // For size_t
#define myId 9
#define numFrames 4
#define node_numData 16

int32_t collect_IMU_temp_F(bool* cancelFrameSend);
int32_t collect_radiator_temp_F(bool* cancelFrameSend);
int32_t collect_humiditySense_temp_F(bool* cancelFrameSend);
int32_t collect_RH(bool* cancelFrameSend);
int32_t collect_posX_m(bool* cancelFrameSend);
int32_t collect_posY_m(bool* cancelFrameSend);
int32_t collect_posZ_m(bool* cancelFrameSend);
int32_t collect_accelX_miliGs(bool* cancelFrameSend);
int32_t collect_accelY_miliGs(bool* cancelFrameSend);
int32_t collect_accelZ_miliGs(bool* cancelFrameSend);
int32_t collect_yaw_degrees(bool* cancelFrameSend);
int32_t collect_pitch_degrees(bool* cancelFrameSend);
int32_t collect_roll_degrees(bool* cancelFrameSend);
int32_t collect_gyroX_deciDegree_p_s(bool* cancelFrameSend);
int32_t collect_gyroY_deciDegree_p_s(bool* cancelFrameSend);
int32_t collect_gyroZ_deciDegree_p_s(bool* cancelFrameSend);

#define dataCollectorsList collect_IMU_temp_F, collect_radiator_temp_F, collect_humiditySense_temp_F, collect_RH, collect_posX_m, collect_posY_m, collect_posZ_m, collect_accelX_miliGs, collect_accelY_miliGs, collect_accelZ_miliGs, collect_yaw_degrees, collect_pitch_degrees, collect_roll_degrees, collect_gyroX_deciDegree_p_s, collect_gyroY_deciDegree_p_s, collect_gyroZ_deciDegree_p_s

#endif