#pragma once

#include "types.hpp"

#include <optional>

class CameraSharedMemory {
public:
    CameraSharedMemory();
    ~CameraSharedMemory();

    bool initialize();

    std::optional<DetectionFrame> readLatest();

private:
    void* mappedMemory = nullptr;
};