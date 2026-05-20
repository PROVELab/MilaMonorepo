#ifndef VITALS_STRUCTS_H
#define VITALS_STRUCTS_H

#include <stdio.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stddef.h> // For offsetof
#include "../../programConstants.h"
#include "pecan/pecan.h" // For simpleDataPoint
#include <stdbool.h> // For bool type

#define R10(x) {x,x,x,x,x,x,x,x,x,x}

#if defined(__cplusplus)
#define STATIC_ASSERT(cond, msg) static_assert((cond), msg)
#else
#define STATIC_ASSERT(cond, msg) _Static_assert((cond), msg)
#endif

typedef struct {
    int32_t min;
    int32_t max;
    int8_t bits;
    int32_t minCritical;
    int32_t maxCritical;
    int32_t minWarning;
    int32_t maxWarning;
    int32_t startingValue;
    uint8_t crit_count_max;
    _Atomic uint8_t crit_count;
} dataPoint;

// This ensures that dataPoint can be safely cast to simpleDataPoint for pecan_unpack.
STATIC_ASSERT(offsetof(dataPoint, min) == offsetof(simpleDataPoint, min), "min offset mismatch");
STATIC_ASSERT(offsetof(dataPoint, max) == offsetof(simpleDataPoint, max), "max offset mismatch");
STATIC_ASSERT(offsetof(dataPoint, bits) == offsetof(simpleDataPoint, bits), "bits offset mismatch");

typedef struct {
    int8_t nodeID;
    int8_t frameID;
    int8_t numData;
    dataPoint *dataInfo; /* Replaced list with dataPoint pointer */
    int8_t flags;
    int8_t dataLocation;
    int8_t consecutiveMisses;
    int32_t dataTimeout;
    bool enableTelemCallback;
    int32_t frequency;
    int32_t (*data)[10]; /* Init to [data points per data =10] [numData for this frame] */
} CANFrame;

typedef struct {
    _Atomic int8_t flags;
    _Atomic int16_t milliSeconds;
    int8_t numFrames;
    CANFrame *CANFrames; 
} vitalsNode;

#endif
