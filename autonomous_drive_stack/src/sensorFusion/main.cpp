#include "sensorFusion/FusionManager.hpp"
#include "sensorFusion/LidarManager.hpp"
#include "sensorFusion/SharedState.hpp"

#include <thread>

int main() {
    SharedState state;

    std::thread lidarThread(
        runLidar,
        std::ref(state)
    );

    std::thread fusionThread(
        runFusion,
        std::ref(state)
    );

    lidarThread.join();
    fusionThread.join();

    return 0;
}