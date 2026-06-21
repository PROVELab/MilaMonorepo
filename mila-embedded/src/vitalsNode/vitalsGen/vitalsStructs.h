#ifndef VITALS_STRUCTS_H
#define VITALS_STRUCTS_H

#include <stdio.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stddef.h> // For offsetof
#include "../../programConstants.h"
#include "pecan/pecan.h" // For simpleDataPoint
#include <stdbool.h> // For bool type

#define R8(x) {x,x,x,x,x,x,x,x}

#if defined(__cplusplus)
#include <atomic>
#define ATOMIC(X) std::atomic< X >
#define STATIC_ASSERT(cond, msg) static_assert((cond), msg)
#else
#define ATOMIC(X) _Atomic X
#define STATIC_ASSERT(cond, msg) _Static_assert((cond), msg)
#endif

        typedef struct {
    uint8_t crit_count_max;
    int32_t minCritical;
    int32_t maxCritical;
    int32_t startingValue;
    int32_t *data; /* pointsPerData samples for this critical datapoint */
    ATOMIC(uint8_t) crit_count;
    bool outlier_present;
    int32_t outlier_slot;
} critical_dataPoint;

typedef struct {
    int32_t min;
    int32_t max;
    int8_t bits;
    int32_t minWarning;
    int32_t maxWarning;
    bool inWarningState;
    critical_dataPoint* criticalStructPtr;
} dataPoint;

// This ensures that dataPoint can be safely cast to simpleDataPoint for pecan_unpack.
STATIC_ASSERT(offsetof(dataPoint, min) == offsetof(simpleDataPoint, min), "min offset mismatch");
STATIC_ASSERT(offsetof(dataPoint, max) == offsetof(simpleDataPoint, max), "max offset mismatch");
STATIC_ASSERT(offsetof(dataPoint, bits) == offsetof(simpleDataPoint, bits), "bits offset mismatch");

typedef struct {
    int8_t nodeIndex;
    int8_t frameID;
    int8_t numData;
    dataPoint *dataInfo; /* Replaced list with dataPoint pointer */
    int32_t dataTimeout;
    int32_t telemetryDivider;
    bool hasCriticalData;
    int32_t telemetryDivider_Count;
    int8_t dataLocation;
    int8_t consecutiveMisses;
} CANFrame;

typedef struct {
    int8_t CAN_ID;
    ATOMIC(int8_t) flags;
    ATOMIC(int16_t) milliSeconds;
    int8_t numFrames;
    CANFrame *CANFrames; 
} vitalsNode;

extern vitalsNode nodes[numberOfNodes];

#endif
