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
#define I2C_MASTER_SCL_IO   22
#define I2C_MASTER_SDA_IO   21
#define I2C_MASTER_NUM      I2C_NUM_0
#define I2C_MASTER_FREQ_HZ  100000

using Imu = espp::Icm20948<espp::icm20948::Interface::I2C>;

/* Internal State Pointers */
static espp::I2c* i2c_ptr = nullptr;
static Imu* imu_ptr = nullptr;

/* Filters */
static constexpr float angle_noise = 0.001f;
static constexpr float rate_noise = 0.1f;
static espp::KalmanFilter<3> kf;

static constexpr float beta = 0.1f;
static espp::MadgwickFilter madgwick_filter(beta);

/* Cached Data Values */
static imu_vector3_t cache_accel = {0, 0, 0};
static imu_vector3_t cache_gyro = {0, 0, 0};
static imu_vector3_t cache_mag = {0, 0, 0};
static float cache_temp = 0.0f;
static imu_orientation_t cache_kalman_ori = {0, 0, 0};
static imu_vector3_t cache_gravity = {0, 0, 0};
static imu_orientation_t cache_madgwick_ori = {0, 0, 0};
static float cache_heading = 0.0f;

/* Filter helper functions */
static Imu::Value kalman_filter_fn(
    float dt,
    const Imu::Value& accel,
    const Imu::Value& gyro,
    const Imu::Value& mag) {

    float accelRoll = atan2(accel.y, accel.z);
    float accelPitch =
        atan2(-accel.x, sqrt(accel.y * accel.y + accel.z * accel.z));
    float accelYaw = atan2(mag.y, mag.x);

    kf.predict({
        espp::deg_to_rad(gyro.x),
        espp::deg_to_rad(gyro.y),
        espp::deg_to_rad(gyro.z)
    }, dt);

    kf.update({
        accelRoll,
        accelPitch,
        accelYaw
    });

    float roll, pitch, yaw;

    const auto& state = kf.get_state();

    roll = state[0];
    pitch = state[1];
    yaw = state[2];

    Imu::Value orientation{};

    orientation.roll = roll;
    orientation.pitch = pitch;
    orientation.yaw = yaw;

    orientation.x = roll;
    orientation.y = pitch;
    orientation.z = yaw;

    return orientation;
}

static Imu::Value madgwick_filter_fn(
    float dt,
    const Imu::Value& accel,
    const Imu::Value& gyro,
    const Imu::Value& mag) {

    madgwick_filter.update(
        dt,
        accel.x,
        accel.y,
        accel.z,
        espp::deg_to_rad(gyro.x),
        espp::deg_to_rad(gyro.y),
        espp::deg_to_rad(gyro.z),
        mag.x,
        mag.y,
        mag.z
    );

    float roll, pitch, yaw;

    madgwick_filter.get_euler(roll, pitch, yaw);

    Imu::Value orientation{};

    orientation.pitch = espp::deg_to_rad(pitch);
    orientation.roll = espp::deg_to_rad(roll);
    orientation.yaw = espp::deg_to_rad(yaw);

    return orientation;
}

/* --------------------------------------------------------------------------
 * C-Compatible API Implementation
 * -------------------------------------------------------------------------- */

extern "C" {

void imu_init(void) {

    espp::Logger logger({
        .tag = "ICM20948 Example",
        .level = espp::Logger::Verbosity::INFO
    });

    logger.info("Starting example!");

    static constexpr auto i2c_port = I2C_NUM_0;
    static constexpr auto i2c_clock_speed = I2C_MASTER_FREQ_HZ;

    static constexpr gpio_num_t i2c_sda =
        (gpio_num_t)I2C_MASTER_SDA_IO;

    static constexpr gpio_num_t i2c_scl =
        (gpio_num_t)I2C_MASTER_SCL_IO;

    logger.info(
        "Creating I2C on port {} with SDA {} and SCL {}",
        i2c_port,
        i2c_sda,
        i2c_scl
    );

    static espp::I2c i2c_instance({
        .port = i2c_port,
        .sda_io_num = i2c_sda,
        .scl_io_num = i2c_scl,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .clk_speed = i2c_clock_speed
    });

    i2c_ptr = &i2c_instance;

    /*
     * ICM-20948 I2C address.
     *
     * AD0 LOW  = 0x68
     * AD0 HIGH = 0x69
     *
     * The SparkFun board is currently expected at 0x68.
     */
    constexpr uint8_t imu_address = 0x69;

    logger.info(
        "Using ICM20948 at fixed I2C address: {:#02x}",
        imu_address
    );

    /*
     * Verify that something responds at 0x68 before
     * attempting to initialize the IMU.
     */
    if (!i2c_ptr->probe_device(imu_address)) {
        logger.error(
            "ICM20948 did not respond at address {:#02x}",
            imu_address
        );
        return;
    }

    logger.info("ICM20948 detected at {:#02x}", imu_address);

    kf.set_process_noise(rate_noise);
    kf.set_measurement_noise(angle_noise);

    Imu::Config config{
        .device_address = imu_address,

        .write = std::bind(
            &espp::I2c::write,
            i2c_ptr,
            std::placeholders::_1,
            std::placeholders::_2,
            std::placeholders::_3
        ),

        .read = std::bind(
            &espp::I2c::read,
            i2c_ptr,
            std::placeholders::_1,
            std::placeholders::_2,
            std::placeholders::_3
        ),

        .imu_config = {
            .accelerometer_range =
                Imu::AccelerometerRange::RANGE_2G,

            .gyroscope_range =
                Imu::GyroscopeRange::RANGE_250DPS,

            .accelerometer_sample_rate_divider = 9,

            .gyroscope_sample_rate_divider = 9,

            .magnetometer_mode =
                Imu::MagnetometerMode::CONTINUOUS_MODE_100_HZ,
        },

        .orientation_filter = kalman_filter_fn,

        .auto_init = true,
    };

    logger.info("Creating IMU");

    vTaskDelay(pdMS_TO_TICKS(100));

    static Imu imu_instance(config);

    imu_ptr = &imu_instance;

    printf("ran imu config!\n");

    vTaskDelay(pdMS_TO_TICKS(100));
}

bool imu_refreshData(void) {

    if (!imu_ptr) {
        return false;
    }

    static constexpr int64_t FRESHNESS_TIMEOUT_MS = 30;

    static int64_t last_update_time_us = 0;

    if ((esp_timer_get_time() - last_update_time_us) <
        (FRESHNESS_TIMEOUT_MS * 1000)) {

        return true;
    }

    std::error_code ec;

    if (!imu_ptr->update(1.0f, ec)) {

        printf(
            "Failed to update IMU: %s\n",
            ec.message().c_str()
        );

        return false;
    }

    auto now = esp_timer_get_time();

    static auto t0 = now;

    auto t1 = now;

    float dt =
        (t1 - t0) / 1000000.0f;

    t0 = t1;

    auto accel =
        imu_ptr->get_accelerometer();

    auto gyro =
        imu_ptr->get_gyroscope();

    auto mag =
        imu_ptr->get_magnetometer();

    auto temp =
        imu_ptr->get_temperature();

    auto orientation =
        imu_ptr->get_orientation();

    auto gravity_vector =
        imu_ptr->get_gravity_vector();

    auto madgwick_orientation =
        madgwick_filter_fn(
            dt,
            accel,
            gyro,
            mag
        );

    cache_accel = {
        (float)accel.x,
        (float)accel.y,
        (float)accel.z
    };

    cache_gyro = {
        (float)gyro.x,
        (float)gyro.y,
        (float)gyro.z
    };

    cache_mag = {
        (float)mag.x,
        (float)mag.y,
        (float)mag.z
    };

    cache_temp = temp;

    cache_kalman_ori = {
        (float)orientation.roll,
        (float)orientation.pitch,
        (float)orientation.yaw
    };

    cache_gravity = {
        (float)gravity_vector.x,
        (float)gravity_vector.y,
        (float)gravity_vector.z
    };

    cache_madgwick_ori = {
        madgwick_orientation.roll,
        madgwick_orientation.pitch,
        madgwick_orientation.yaw
    };

    cache_heading =
        fmod(
            (cache_madgwick_ori.yaw *
             180.0f /
             static_cast<float>(M_PI))
            + 360.0f,
            360.0f
        );

    last_update_time_us =
        esp_timer_get_time();

    return true;
}

imu_vector3_t read_accelerometer(void) {
    return cache_accel;
}

imu_vector3_t read_gyroscope(void) {
    return cache_gyro;
}

imu_vector3_t read_magnetometer(void) {
    return cache_mag;
}

float read_temperature(void) {
    return cache_temp;
}

imu_orientation_t read_kalman_orientation(void) {
    return cache_kalman_ori;
}

imu_vector3_t read_gravity_vector(void) {
    return cache_gravity;
}

imu_orientation_t read_madgwick_orientation(void) {
    return cache_madgwick_ori;
}

float read_heading(void) {
    return cache_heading;
}

} // extern "C"