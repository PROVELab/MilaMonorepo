/**
 * @file sensorRecvCallbacks.cpp
 * @brief Skeleton implementations for command callbacks.
 * NOTE: You may move these implementations to your main.c/cpp file for convenience.
 */

#include "myDefines.hpp"

#include "../coolant.h"

void onsetCoolantDutyCycle(setCoolantDutyCycle_args_t args) {
    // TODO: Implement logic for setCoolantDutyCycle
    setCoolantDutyCycle(args.dutyCycle);
}

void onsetCoolantFrequency_HZ(setCoolantFrequency_HZ_args_t args) {
    // TODO: Implement logic for setCoolantFrequency_HZ
}

