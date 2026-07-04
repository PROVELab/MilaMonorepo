#ifndef IMU_MANAGER_H
#define IMU_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* Data structures to safely pass C++ data to C */
typedef struct {
    float x;
    float y;
    float z;
} imu_vector3_t;

typedef struct {
    float roll;
    float pitch;
    float yaw;
} imu_orientation_t;

/* Initialization and update functions */
void imu_init(void);
bool imu_refreshData(void); /* Blocks/refreshes IMU data. Returns true on success */

/* Getter functions */
imu_vector3_t read_accelerometer(void);
imu_vector3_t read_gyroscope(void);
imu_vector3_t read_magnetometer(void);
float read_temperature(void);

imu_orientation_t read_kalman_orientation(void);
imu_vector3_t read_gravity_vector(void);
imu_orientation_t read_madgwick_orientation(void);
float read_heading(void);

imu_vector3_t read_position(void);
imu_vector3_t read_velocity(void);
void imu_reset_position(void);

#ifdef __cplusplus
}
#endif

#endif /* IMU_MANAGER_H */