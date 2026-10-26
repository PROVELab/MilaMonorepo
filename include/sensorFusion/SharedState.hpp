#pragma once

#include "types.hpp"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>

struct SharedState {
    std::mutex lidarMutex;
    std::condition_variable lidarCv;

    std::deque<std::shared_ptr<const LidarFrame>> lidarFrames;

    std::atomic<bool> running{true};
};