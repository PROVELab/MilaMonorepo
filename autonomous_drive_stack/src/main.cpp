#include <iostream>
#include <ouster/core/sensor_config.h>
#include <ouster/core/types.h>
#include <ouster/sensor/client.h>
int main() {
    ouster::sdk::core::SensorConfig config = ouster::sdk::sensor::get_config(SENSOR_HOSTNAME);
    std::cerr << "Sensor config of " << SENSOR_HOSTNAME << ":\n" << ouster::sdk::core::to_string(config) << std::endl;
}
