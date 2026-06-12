#include <stdio.h>
#include <stdint.h>
#include "vitalsStructs.h"

#define R8(x) {x,x,x,x,x,x,x,x}
// Node 0: precharge
dataPoint n0f0DPs [5]={
    {.min=-100, .max=923, .bits=10, .minCritical=80, .maxCritical=923, .minWarning=120, .maxWarning=923, .startingValue=0, .crit_count_max=1, .crit_count=0, .inWarningState=0, .outlier_present=0, .outlier_slot=0},
    {.min=-100, .max=923, .bits=10, .minCritical=80, .maxCritical=923, .minWarning=120, .maxWarning=923, .startingValue=0, .crit_count_max=1, .crit_count=0, .inWarningState=0, .outlier_present=0, .outlier_slot=0},
    {.min=0, .max=2, .bits=2, .minCritical=0, .maxCritical=2, .minWarning=0, .maxWarning=2, .startingValue=2, .crit_count_max=0, .crit_count=0, .inWarningState=0, .outlier_present=0, .outlier_slot=0},
    {.min=0, .max=2, .bits=2, .minCritical=0, .maxCritical=2, .minWarning=0, .maxWarning=2, .startingValue=2, .crit_count_max=0, .crit_count=0, .inWarningState=0, .outlier_present=0, .outlier_slot=0},
    {.min=0, .max=1, .bits=1, .minCritical=0, .maxCritical=1, .minWarning=0, .maxWarning=1, .startingValue=0, .crit_count_max=0, .crit_count=0, .inWarningState=0, .outlier_present=0, .outlier_slot=0},
};

int32_t n0f0Data[5][8]={R8(0),R8(0),R8(2),R8(2),R8(0)};

CANFrame n0[1]={
    {.nodeID=3, .frameID=0, .numData=5, .dataTimeout=500, .telemetryDivider=1, .telemetryDivider_Count=0, .dataLocation=0, .consecutiveMisses=0, .data=n0f0Data , .dataInfo=n0f0DPs},
};

// Node 1: pedalSensor
dataPoint n1f0DPs [5]={
    {.min=-30000, .max=35000, .bits=16, .minCritical=-30000, .maxCritical=35000, .minWarning=4700, .maxWarning=7100, .startingValue=6000, .crit_count_max=0, .crit_count=0, .inWarningState=0, .outlier_present=0, .outlier_slot=0},
    {.min=-20, .max=120, .bits=8, .minCritical=5, .maxCritical=95, .minWarning=20, .maxWarning=80, .startingValue=30, .crit_count_max=1, .crit_count=0, .inWarningState=0, .outlier_present=0, .outlier_slot=0},
    {.min=-20, .max=120, .bits=8, .minCritical=5, .maxCritical=95, .minWarning=20, .maxWarning=80, .startingValue=30, .crit_count_max=1, .crit_count=0, .inWarningState=0, .outlier_present=0, .outlier_slot=0},
    {.min=-20, .max=120, .bits=8, .minCritical=-20, .maxCritical=120, .minWarning=-10, .maxWarning=110, .startingValue=-1, .crit_count_max=0, .crit_count=0, .inWarningState=0, .outlier_present=0, .outlier_slot=0},
    {.min=0, .max=1, .bits=1, .minCritical=0, .maxCritical=1, .minWarning=0, .maxWarning=1, .startingValue=0, .crit_count_max=0, .crit_count=0, .inWarningState=0, .outlier_present=0, .outlier_slot=0},
};

int32_t n1f0Data[5][8]={R8(6000),R8(30),R8(30),R8(-1),R8(0)};

CANFrame n1[1]={
    {.nodeID=8, .frameID=1, .numData=5, .dataTimeout=5000, .telemetryDivider=1, .telemetryDivider_Count=0, .dataLocation=0, .consecutiveMisses=0, .data=n1f0Data , .dataInfo=n1f0DPs},
};

// Node 2: IMU
dataPoint n2f0DPs [4]={
    {.min=-100, .max=923, .bits=10, .minCritical=-100, .maxCritical=923, .minWarning=-100, .maxWarning=923, .startingValue=67, .crit_count_max=0, .crit_count=0, .inWarningState=0, .outlier_present=0, .outlier_slot=0},
    {.min=-100, .max=923, .bits=10, .minCritical=-100, .maxCritical=923, .minWarning=-100, .maxWarning=923, .startingValue=67, .crit_count_max=0, .crit_count=0, .inWarningState=0, .outlier_present=0, .outlier_slot=0},
    {.min=-100, .max=923, .bits=10, .minCritical=-100, .maxCritical=923, .minWarning=-100, .maxWarning=923, .startingValue=67, .crit_count_max=0, .crit_count=0, .inWarningState=0, .outlier_present=0, .outlier_slot=0},
    {.min=-55, .max=200, .bits=8, .minCritical=-55, .maxCritical=200, .minWarning=0, .maxWarning=100, .startingValue=1, .crit_count_max=0, .crit_count=0, .inWarningState=0, .outlier_present=0, .outlier_slot=0},
};

dataPoint n2f1DPs [3]={
    {.min=-1048576, .max=1048575, .bits=21, .minCritical=-1048576, .maxCritical=1048575, .minWarning=-1048576, .maxWarning=1048575, .startingValue=0, .crit_count_max=0, .crit_count=0, .inWarningState=0, .outlier_present=0, .outlier_slot=0},
    {.min=-1048576, .max=1048575, .bits=21, .minCritical=-1048576, .maxCritical=1048575, .minWarning=-1048576, .maxWarning=1048575, .startingValue=0, .crit_count_max=0, .crit_count=0, .inWarningState=0, .outlier_present=0, .outlier_slot=0},
    {.min=-1048576, .max=1048575, .bits=21, .minCritical=-1048576, .maxCritical=1048575, .minWarning=-1048576, .maxWarning=1048575, .startingValue=0, .crit_count_max=0, .crit_count=0, .inWarningState=0, .outlier_present=0, .outlier_slot=0},
};

dataPoint n2f2DPs [5]={
    {.min=-32768, .max=32767, .bits=16, .minCritical=-32768, .maxCritical=32767, .minWarning=-32768, .maxWarning=32767, .startingValue=0, .crit_count_max=0, .crit_count=0, .inWarningState=0, .outlier_present=0, .outlier_slot=0},
    {.min=-32768, .max=32767, .bits=16, .minCritical=-32768, .maxCritical=32767, .minWarning=-32768, .maxWarning=32767, .startingValue=0, .crit_count_max=0, .crit_count=0, .inWarningState=0, .outlier_present=0, .outlier_slot=0},
    {.min=-128, .max=127, .bits=8, .minCritical=-128, .maxCritical=127, .minWarning=-128, .maxWarning=127, .startingValue=0, .crit_count_max=0, .crit_count=0, .inWarningState=0, .outlier_present=0, .outlier_slot=0},
    {.min=-128, .max=127, .bits=8, .minCritical=-128, .maxCritical=127, .minWarning=-128, .maxWarning=127, .startingValue=0, .crit_count_max=0, .crit_count=0, .inWarningState=0, .outlier_present=0, .outlier_slot=0},
    {.min=-128, .max=127, .bits=8, .minCritical=-128, .maxCritical=127, .minWarning=-128, .maxWarning=127, .startingValue=0, .crit_count_max=0, .crit_count=0, .inWarningState=0, .outlier_present=0, .outlier_slot=0},
};

int32_t n2f0Data[4][8]={R8(67),R8(67),R8(67),R8(1)};

int32_t n2f1Data[3][8]={R8(0),R8(0),R8(0)};

int32_t n2f2Data[5][8]={R8(0),R8(0),R8(0),R8(0),R8(0)};

CANFrame n2[3]={
    {.nodeID=9, .frameID=2, .numData=4, .dataTimeout=500, .telemetryDivider=1, .telemetryDivider_Count=0, .dataLocation=0, .consecutiveMisses=0, .data=n2f0Data , .dataInfo=n2f0DPs},
    {.nodeID=9, .frameID=3, .numData=3, .dataTimeout=5000, .telemetryDivider=1, .telemetryDivider_Count=0, .dataLocation=0, .consecutiveMisses=0, .data=n2f1Data , .dataInfo=n2f1DPs},
    {.nodeID=9, .frameID=4, .numData=5, .dataTimeout=5000, .telemetryDivider=1, .telemetryDivider_Count=0, .dataLocation=0, .consecutiveMisses=0, .data=n2f2Data , .dataInfo=n2f2DPs},
};

// Node 3: powerDistribution
dataPoint n3f0DPs [2]={
    {.min=-22768, .max=42767, .bits=16, .minCritical=-22768, .maxCritical=42767, .minWarning=-22768, .maxWarning=42767, .startingValue=0, .crit_count_max=0, .crit_count=0, .inWarningState=0, .outlier_present=0, .outlier_slot=0},
    {.min=-32768, .max=32767, .bits=16, .minCritical=-32768, .maxCritical=32767, .minWarning=-32768, .maxWarning=32767, .startingValue=0, .crit_count_max=0, .crit_count=0, .inWarningState=0, .outlier_present=0, .outlier_slot=0},
};

dataPoint n3f1DPs [5]={
    {.min=-32768, .max=32767, .bits=16, .minCritical=-32768, .maxCritical=32767, .minWarning=-32768, .maxWarning=32767, .startingValue=0, .crit_count_max=0, .crit_count=0, .inWarningState=0, .outlier_present=0, .outlier_slot=0},
    {.min=-32768, .max=32767, .bits=16, .minCritical=-32768, .maxCritical=32767, .minWarning=-32768, .maxWarning=32767, .startingValue=0, .crit_count_max=0, .crit_count=0, .inWarningState=0, .outlier_present=0, .outlier_slot=0},
    {.min=0, .max=255, .bits=8, .minCritical=0, .maxCritical=255, .minWarning=0, .maxWarning=255, .startingValue=0, .crit_count_max=0, .crit_count=0, .inWarningState=0, .outlier_present=0, .outlier_slot=0},
    {.min=0, .max=100, .bits=7, .minCritical=0, .maxCritical=100, .minWarning=0, .maxWarning=100, .startingValue=0, .crit_count_max=0, .crit_count=0, .inWarningState=0, .outlier_present=0, .outlier_slot=0},
    {.min=0, .max=1, .bits=1, .minCritical=0, .maxCritical=1, .minWarning=0, .maxWarning=1, .startingValue=0, .crit_count_max=0, .crit_count=0, .inWarningState=0, .outlier_present=0, .outlier_slot=0},
};

int32_t n3f0Data[2][8]={R8(0),R8(0)};

int32_t n3f1Data[5][8]={R8(0),R8(0),R8(0),R8(0),R8(0)};

CANFrame n3[2]={
    {.nodeID=10, .frameID=5, .numData=2, .dataTimeout=500, .telemetryDivider=1, .telemetryDivider_Count=0, .dataLocation=0, .consecutiveMisses=0, .data=n3f0Data , .dataInfo=n3f0DPs},
    {.nodeID=10, .frameID=6, .numData=5, .dataTimeout=1000, .telemetryDivider=1, .telemetryDivider_Count=0, .dataLocation=0, .consecutiveMisses=0, .data=n3f1Data , .dataInfo=n3f1DPs},
};

// Node 4: APSensorESP
dataPoint n4f0DPs [1]={
    {.min=-2147483648, .max=2147483647, .bits=32, .minCritical=5, .maxCritical=95, .minWarning=20, .maxWarning=80, .startingValue=50, .crit_count_max=1, .crit_count=0, .inWarningState=0, .outlier_present=0, .outlier_slot=0},
};

int32_t n4f0Data[1][8]={R8(50)};

CANFrame n4[1]={
    {.nodeID=11, .frameID=7, .numData=1, .dataTimeout=1500, .telemetryDivider=1, .telemetryDivider_Count=0, .dataLocation=0, .consecutiveMisses=0, .data=n4f0Data , .dataInfo=n4f0DPs},
};

// vitalsData *nodes;
vitalsNode nodes [5]={
    {.flags=0, .milliSeconds=0, .numFrames=1, .CANFrames=n0},
    {.flags=0, .milliSeconds=0, .numFrames=1, .CANFrames=n1},
    {.flags=0, .milliSeconds=0, .numFrames=3, .CANFrames=n2},
    {.flags=0, .milliSeconds=0, .numFrames=2, .CANFrames=n3},
    {.flags=0, .milliSeconds=0, .numFrames=1, .CANFrames=n4},
};
int16_t missingIDs[]={4, 5, 6, 7};
