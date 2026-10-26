#include "sensorFusion/CameraSharedMemory.hpp"

CameraSharedMemory::CameraSharedMemory() = default;

CameraSharedMemory::~CameraSharedMemory() {
    // Later:
    // munmap(...)
}

bool CameraSharedMemory::initialize() {
    // Later:
    // mmap(...)
    // initialize process-shared mutex
    // initialize process-shared condition variable

    return true;
}

std::optional<DetectionFrame>
CameraSharedMemory::readLatest() {
    // Later:
    // lock shared pthread mutex
    // copy camera frame
    // unlock
    return std::nullopt;
}