#include "sensorFusion/FusionManager.hpp"

#include <iostream>
#include <memory>

void runFusion(SharedState& state) {
    while (state.running) {

        std::shared_ptr<const LidarFrame> frame;

        {
            std::unique_lock<std::mutex> lock(state.lidarMutex);

            state.lidarCv.wait(lock, [&] {
                return !state.lidarFrames.empty()
                    || !state.running;
            });

            if (!state.running) {
                break;
            }

            frame = state.lidarFrames.back();

            // Optional:
            // discard older frames so fusion stays current
            state.lidarFrames.clear();
        }

        // Important:
        // do expensive work OUTSIDE the mutex
        std::cout
            << "Fusion received LiDAR frame with "
            << frame->points.size()
            << " points\n";

        // Later:
        // projectLidarToCamera(...)
        // resolveDepth(...)
        // matchCameraAndLidarByTimestamp(...)
    }
}