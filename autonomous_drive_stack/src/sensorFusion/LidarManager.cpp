#include "sensorFusion/LidarManager.hpp"

#include <chrono>
#include <thread>

static uint64_t getTimestampNs() {
    const auto now = std::chrono::steady_clock::now();

    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()
    ).count();
}

void runLidar(SharedState& state) {
    while (state.running) {

        auto frame = std::make_shared<LidarFrame>();

        frame->timestamp_ns = getTimestampNs();

        // Fake point cloud for now
        frame->points.push_back({1.0f, 2.0f, 3.0f});
        frame->points.push_back({4.0f, 5.0f, 6.0f});

        {
            std::lock_guard<std::mutex> lock(state.lidarMutex);

            state.lidarFrames.push_back(frame);

            // Keep the buffer bounded
            if (state.lidarFrames.size() > 5) {
                state.lidarFrames.pop_front();
            }
        }

        state.lidarCv.notify_one();

        // Simulate 10 Hz LiDAR
        std::this_thread::sleep_for(
            std::chrono::milliseconds(100)
        );
    }
}