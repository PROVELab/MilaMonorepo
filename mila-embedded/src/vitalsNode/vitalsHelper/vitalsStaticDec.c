#include <stdio.h>
#include <stdint.h>
#include "vitalsStaticDec.h"
#include "vitalsStructs.h"

#define R10(x) {x,x,x,x,x,x,x,x,x,x}
// Node 0: pedalSensor
dataPoint n0f0DPs [3]={
    {.bitLength=16, .minCritical=4500, .maxCritical=7500, .min=-30000, .max=35000, .minWarning=4900, .maxWarning=7100, .startingValue=6000, .crit_count=0},
    {.bitLength=8, .minCritical=5, .maxCritical=95, .min=-20, .max=120, .minWarning=20, .maxWarning=80, .startingValue=30, .crit_count=0},
    {.bitLength=8, .minCritical=5, .maxCritical=95, .min=-20, .max=120, .minWarning=20, .maxWarning=80, .startingValue=30, .crit_count=0},
};

int32_t n0f0Data[3][10]={R10(6000),R10(30),R10(30)};

CANFrame n0[1]={
    {.nodeID=8, .frameID=0, .numData=3, .flags=0, .dataLocation=0, .consecutiveMisses=0, .dataTimeout=1000, .data=n0f0Data , .dataInfo=n0f0DPs},
};

// Node 1: APSensorArduino
dataPoint n1f0DPs [1]={
    {.bitLength=32, .minCritical=5, .maxCritical=95, .min=-2147483648, .max=2147483647, .minWarning=20, .maxWarning=80, .startingValue=50, .crit_count=0},
};

int32_t n1f0Data[1][10]={R10(50)};

CANFrame n1[1]={
    {.nodeID=10, .frameID=1, .numData=1, .flags=0, .dataLocation=0, .consecutiveMisses=0, .dataTimeout=1500, .data=n1f0Data , .dataInfo=n1f0DPs},
};

// Node 2: APSensorESP
dataPoint n2f0DPs [1]={
    {.bitLength=32, .minCritical=5, .maxCritical=95, .min=-2147483648, .max=2147483647, .minWarning=20, .maxWarning=80, .startingValue=50, .crit_count=0},
};

int32_t n2f0Data[1][10]={R10(50)};

CANFrame n2[1]={
    {.nodeID=11, .frameID=2, .numData=1, .flags=0, .dataLocation=0, .consecutiveMisses=0, .dataTimeout=1500, .data=n2f0Data , .dataInfo=n2f0DPs},
};

// vitalsData *nodes;
vitalsNode nodes [3]={
    {.flags=0, .milliSeconds=0, .numFrames=1, .CANFrames=n0},
    {.flags=0, .milliSeconds=0, .numFrames=1, .CANFrames=n1},
    {.flags=0, .milliSeconds=0, .numFrames=1, .CANFrames=n2},
};
int16_t missingIDs[]={9};
