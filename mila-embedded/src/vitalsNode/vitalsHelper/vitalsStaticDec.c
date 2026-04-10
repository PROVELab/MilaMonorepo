#include <stdio.h>
#include <stdint.h>
#include "vitalsStaticDec.h"
#include "vitalsStructs.h"

#define R10(x) {x,x,x,x,x,x,x,x,x,x}
// Node 0: APSensorArduino
dataPoint n0f0DPs [1]={
    {.bitLength=32, .minCritical=5, .maxCritical=95, .min=-2147483648, .max=2147483647, .minWarning=20, .maxWarning=80, .startingValue=50, .crit_count=0},
};

int32_t n0f0Data[1][10]={R10(50)};

CANFrame n0[1]={
    {.nodeID=10, .frameID=0, .numData=1, .flags=0, .dataLocation=0, .consecutiveMisses=0, .dataTimeout=1500, .data=n0f0Data , .dataInfo=n0f0DPs},
};

// vitalsData *nodes;
vitalsNode nodes [1]={
    {.flags=0, .milliSeconds=0, .numFrames=1, .CANFrames=n0},
};
int16_t missingIDs[]={};
