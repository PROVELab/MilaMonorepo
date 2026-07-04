#include "imu_manager.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <chrono>

#include "freertos/FreeRTOS.h"  
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/twai.h"

#include "kalman_filter.hpp"
#include "madgwick_filter.hpp"
#include "icm20948.hpp"
#include "i2c.hpp"

using namespace std::chrono_literals;

/* I2C Constants */
#define I2C_MASTER_SCL_IO  22        
#define I2C_MASTER_SDA_IO  21        
#define I2C_MASTER_NUM     I2C_NUM_0 
#define I2C_MASTER_FREQ_HZ 100000    

using Imu = espp::Icm20948<espp::icm20948::Interface::I2C>;

/* Internal State Pointers (Point to static memory) */
static espp::I2c* i2c_ptr = nullptr;
static Imu* imu_ptr = nullptr;

/* Filters (Statically allocated) */
static constexpr float angle_noise = 0.001f;
static constexpr float rate_noise = 0.1f;
static espp::KalmanFilter<3> kf;

static constexpr float beta = 0.1f; 
static espp::MadgwickFilter madgwick_filter(beta);

/* Cached Data Values (Statically allocated) */
static imu_vector3_t cache_accel = {0, 0, 0};
static imu_vector3_t cache_gyro = {0, 0, 0};
static imu_vector3_t cache_mag = {0, 0, 0};
static float cache_temp = 0.0f;
static imu_orientation_t cache_kalman_ori = {0, 0, 0};
static imu_vector3_t cache_gravity = {0, 0, 0};
static imu_orientation_t cache_madgwick_ori = {0, 0, 0};
static float cache_heading = 0.0f;

static imu_vector3_t cache_velocity = {0, 0, 0};
static imu_vector3_t cache_position = {0, 0, 0};
static imu_vector3_t prev_world_accel = {0, 0, 0}; /* for trapezoidal integration */

/* Bias calibration */
static constexpr int CALIBRATION_SAMPLES = 50; /* ~at 30ms/sample (FRESHNESS_TIMEOUT_MS) ~1.5s */
static bool bias_calibrated = false;
static int calibration_count = 0;
static imu_vector3_t accel_bias = {0, 0, 0};
static imu_vector3_t accel_bias_accum = {0, 0, 0};

//Zero velocity update thresholds
static constexpr float ZUPT_ACCEL_THRESHOLD = 0.03f;   /*DON"T HAVE IMU TO SEE WHAT THE THRESHOLD SHOULD BE.*/
static constexpr float ZUPT_GYRO_THRESHOLD = 2.0f;     /*CHANGE TO ACCURATE NUMBER ONCE IMU THRESHOLD TESTING IS DONE */


/* Position helper functions */
static inline float vec3_mag(const imu_vector3_t &v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

static imu_vector3_t rotate_body_to_world(const imu_vector3_t &v, float roll, float pitch, float yaw) {
    float cr = cosf(roll),  sr = sinf(roll);
    float cp = cosf(pitch), sp = sinf(pitch);
    float cy = cosf(yaw),   sy = sinf(yaw);

    // R = Rz(yaw) * Ry(pitch) * Rx(roll)
    float r00 = cy * cp;
    float r01 = cy * sp * sr - sy * cr;
    float r02 = cy * sp * cr + sy * sr;

    float r10 = sy * cp;
    float r11 = sy * sp * sr + cy * cr;
    float r12 = sy * sp * cr - cy * sr;

    float r20 = -sp;
    float r21 = cp * sr;
    float r22 = cp * cr;

    imu_vector3_t out;
    out.x = r00 * v.x + r01 * v.y + r02 * v.z;
    out.y = r10 * v.x + r11 * v.y + r12 * v.z;
    out.z = r20 * v.x + r21 * v.y + r22 * v.z;
    return out;
}

void imu_reset_position(void) {
    cache_position = {0, 0, 0};
    cache_velocity = {0, 0, 0};
    prev_world_accel = {0, 0, 0};
    accel_bias = {0, 0, 0};
    accel_bias_accum = {0, 0, 0};
    calibration_count = 0;
    bias_calibrated = false; // forces re-calibration assuming vehicle is still at the moment of reset
}

/* Filter helper functions */
static Imu::Value kalman_filter_fn(float dt, const Imu::Value &accel, const Imu::Value &gyro, const Imu::Value &mag) {
    float accelRoll = atan2(accel.y, accel.z);
    float accelPitch = atan2(-accel.x, sqrt(accel.y * accel.y + accel.z * accel.z));
    float accelYaw = atan2(mag.y, mag.x);
    
    kf.predict({espp::deg_to_rad(gyro.x), espp::deg_to_rad(gyro.y), espp::deg_to_rad(gyro.z)}, dt);
    kf.update({accelRoll, accelPitch, accelYaw});
    
    float roll, pitch, yaw;
    const auto &state = kf.get_state();
    roll  = state[0];
    pitch = state[1];
    yaw   = state[2];
    
    Imu::Value orientation{};
    orientation.roll = roll;
    orientation.pitch = pitch;
    orientation.yaw = yaw;
    orientation.x = roll;
    orientation.y = pitch;
    orientation.z = yaw;
    
    return orientation;
}

static Imu::Value madgwick_filter_fn(float dt, const Imu::Value &accel, const Imu::Value &gyro, const Imu::Value &mag) {
    madgwick_filter.update(dt, accel.x, accel.y, accel.z, espp::deg_to_rad(gyro.x), espp::deg_to_rad(gyro.y),
                           espp::deg_to_rad(gyro.z), mag.x, mag.y, mag.z);
    float roll, pitch, yaw;
    madgwick_filter.get_euler(roll, pitch, yaw);
    
    Imu::Value orientation{};
    orientation.pitch = espp::deg_to_rad(pitch);
    orientation.roll = espp::deg_to_rad(roll);
    orientation.yaw = espp::deg_to_rad(yaw);
    
    return orientation;
}

/* -----------------------------------------------------------------------------
 * C-Compatible API Implementation
 * -------------------------------------------------------------------------- */
extern "C" {

void imu_init(void) {
    espp::Logger logger({.tag = "ICM20948 Example", .level = espp::Logger::Verbosity::INFO});
    logger.info("Starting example!");

    static constexpr auto i2c_port = I2C_NUM_0;
    static constexpr auto i2c_clock_speed = I2C_MASTER_FREQ_HZ;
    static constexpr gpio_num_t i2c_sda = (gpio_num_t)I2C_MASTER_SDA_IO;
    static constexpr gpio_num_t i2c_scl = (gpio_num_t)I2C_MASTER_SCL_IO;

    logger.info("Creating I2C on port {} with SDA {} and SCL {}", i2c_port, i2c_sda, i2c_scl);
    
    // Allocate I2C statically. It initializes on the first call to imu_init().
    static espp::I2c i2c_instance({
        .port = i2c_port,
        .sda_io_num = i2c_sda,
        .scl_io_num = i2c_scl,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .clk_speed = i2c_clock_speed
    });
    i2c_ptr = &i2c_instance;

    // Use a fixed stack array instead of std::vector to avoid heap allocations
    uint8_t found_addresses[128];
    uint8_t num_found = 0;
    for (uint8_t address = 0; address < 128; address++) {
        if (i2c_ptr->probe_device(address)) {
            found_addresses[num_found++] = address;
        }
    }
    
    if (num_found == 0) {
        logger.error("No IMU devices found on I2C bus!");
        return;
    }

    uint8_t imu_address = found_addresses[0];
    logger.info("Using IMU at address: {:#02x}", imu_address);

    kf.set_process_noise(rate_noise);
    kf.set_measurement_noise(angle_noise);

    Imu::Config config{
        .device_address = imu_address,
        .write = std::bind(&espp::I2c::write, i2c_ptr, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
        .read = std::bind(&espp::I2c::read, i2c_ptr, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
        .imu_config = {
            .accelerometer_range = Imu::AccelerometerRange::RANGE_2G,
            .gyroscope_range = Imu::GyroscopeRange::RANGE_250DPS,
            .accelerometer_sample_rate_divider = 9, 
            .gyroscope_sample_rate_divider = 9,     
            .magnetometer_mode = Imu::MagnetometerMode::CONTINUOUS_MODE_100_HZ,
        },
        .orientation_filter = kalman_filter_fn,
        .auto_init = true,
    };

    logger.info("Creating IMU");
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Allocate IMU statically. It initializes on the first call to imu_init().
    static Imu imu_instance(config);
    imu_ptr = &imu_instance;
    
    printf("ran imu config!\n");
    vTaskDelay(pdMS_TO_TICKS(100));
}

bool imu_refreshData(void) {
    if (!imu_ptr) return false;

    static constexpr int64_t FRESHNESS_TIMEOUT_MS = 30;
    static int64_t last_update_time_us = 0;    
    if ((esp_timer_get_time() - last_update_time_us) < (FRESHNESS_TIMEOUT_MS * 1000)) {
        return true;  //last read was within FRESHNESS_TIMEOUT ago
    }

    //update new value
    std::error_code ec;
    if (!imu_ptr->update(1.0f, ec)) {
        printf("Failed to update IMU: %s\n", ec.message().c_str());
        return false;
    }

    // Time calculations
    auto now = esp_timer_get_time(); 
    static auto t0 = now;
    auto t1 = now;
    float dt = (t1 - t0) / 1000000.0f; 
    t0 = t1;

    // Fetch values from hardware class
    auto accel = imu_ptr->get_accelerometer();
    auto gyro = imu_ptr->get_gyroscope();
    auto mag = imu_ptr->get_magnetometer();
    auto temp = imu_ptr->get_temperature();
    auto orientation = imu_ptr->get_orientation();
    auto gravity_vector = imu_ptr->get_gravity_vector();
    auto madgwick_orientation = madgwick_filter_fn(dt, accel, gyro, mag);

    // Populate static caches
    cache_accel = {(float)accel.x, (float)accel.y, (float)accel.z};
    cache_gyro  = {(float)gyro.x, (float)gyro.y, (float)gyro.z};
    cache_mag   = {(float)mag.x, (float)mag.y, (float)mag.z};
    cache_temp  = temp;
    
    cache_kalman_ori = {(float)orientation.roll, (float)orientation.pitch, (float)orientation.yaw};
    cache_gravity = {(float)gravity_vector.x, (float)gravity_vector.y, (float)gravity_vector.z};
    
    cache_madgwick_ori = {madgwick_orientation.roll, madgwick_orientation.pitch, madgwick_orientation.yaw};
    
    cache_heading = fmod((cache_madgwick_ori.yaw * 180.0f / static_cast<float>(M_PI)) + 360.0f, 360.0f);

    /*Dead reckoning IMU position calculations*/
    if (!bias_calibrated) {
        accel_bias_accum.x += cache_accel.x - cache_gravity.x;
        accel_bias_accum.y += cache_accel.y - cache_gravity.y;
        accel_bias_accum.z += cache_accel.z - cache_gravity.z;
        calibration_count++;

        if (calibration_count >= CALIBRATION_SAMPLES) {
            accel_bias.x = accel_bias_accum.x / calibration_count;
            accel_bias.y = accel_bias_accum.y / calibration_count;
            accel_bias.z = accel_bias_accum.z / calibration_count;
            bias_calibrated = true;
            printf("IMU position: bias calibration done (bias x=%.4f y=%.4f z=%.4f g)\n",
                   accel_bias.x, accel_bias.y, accel_bias.z);
        }
    } else {
        imu_vector3_t linear_accel_g = {
            cache_accel.x - cache_gravity.x - accel_bias.x,
            cache_accel.y - cache_gravity.y - accel_bias.y,
            cache_accel.z - cache_gravity.z - accel_bias.z
        };

        // Zero update velocity point
        bool is_stationary = (vec3_mag(linear_accel_g) < ZUPT_ACCEL_THRESHOLD) &&
                              (vec3_mag(cache_gyro) < ZUPT_GYRO_THRESHOLD);

        if (is_stationary) {
            cache_velocity = {0, 0, 0};
            prev_world_accel = {0, 0, 0}; // avoid a stale jump in trapezoidal integration when motion resumes
        } else if (dt > 0.0f && dt < 1.0f) { // sanity bound on dt (skip first-call / stale dt)
            static constexpr float G_TO_MPS2 = 9.80665f;
            imu_vector3_t linear_accel_mps2 = {
                linear_accel_g.x * G_TO_MPS2,
                linear_accel_g.y * G_TO_MPS2,
                linear_accel_g.z * G_TO_MPS2
            };

            imu_vector3_t world_accel = rotate_body_to_world(
                linear_accel_mps2,
                cache_madgwick_ori.roll,
                cache_madgwick_ori.pitch,
                cache_madgwick_ori.yaw
            );

            // Trapezoidal Integration
            imu_vector3_t new_velocity = {
                cache_velocity.x + dt * 0.5f * (prev_world_accel.x + world_accel.x),
                cache_velocity.y + dt * 0.5f * (prev_world_accel.y + world_accel.y),
                cache_velocity.z + dt * 0.5f * (prev_world_accel.z + world_accel.z)
            };

            cache_position.x += dt * 0.5f * (cache_velocity.x + new_velocity.x);
            cache_position.y += dt * 0.5f * (cache_velocity.y + new_velocity.y);
            cache_position.z += dt * 0.5f * (cache_velocity.z + new_velocity.z);

            cache_velocity = new_velocity;
            prev_world_accel = world_accel;
        }
    }

    last_update_time_us = esp_timer_get_time();

    return true;
}

imu_vector3_t read_accelerometer(void) { return cache_accel; }
imu_vector3_t read_gyroscope(void) { return cache_gyro; }
imu_vector3_t read_magnetometer(void) { return cache_mag; }
float read_temperature(void) { return cache_temp; }
imu_orientation_t read_kalman_orientation(void) { return cache_kalman_ori; }
imu_vector3_t read_gravity_vector(void) { return cache_gravity; }
imu_orientation_t read_madgwick_orientation(void) { return cache_madgwick_ori; }
float read_heading(void) { return cache_heading; }

imu_vector3_t read_position(void) { return cache_position; }
imu_vector3_t read_velocity(void) { return cache_velocity; }


} // extern "C"


// void imu_init(void){return;}
// bool imu_refreshData(void){return true;} /* Blocks/refreshes IMU data. Returns true on success */

// imu_vector3_t read_accelerometer(void) { return {0};} 
// imu_vector3_t read_gyroscope(void) { return {0}; } 
// imu_vector3_t read_magnetometer(void) { return {0}; }
// float read_temperature(void) { return 0; }
// imu_orientation_t read_kalman_orientation(void) { return {0}; }
// imu_vector3_t read_gravity_vector(void) { return {0}; }
// imu_orientation_t read_madgwick_orientation(void) { return {0}; }
// float read_heading(void) { return 0; }