#include <stdio.h>
#include <stdint.h>
#include "vitalsStructs.h"

#define R8(x) {x,x,x,x,x,x,x,x}
// Node 0: precharge
int32_t n0f0CriticalData[3][pointsPerData]={R8(200),R8(10),R8(7)};

critical_dataPoint n0f0CriticalDPs [3]={
    {.crit_count_max=1, .minCritical=90, .maxCritical=923, .startingValue=200, .data=n0f0CriticalData[0], .crit_count=0, .outlier_present=0, .outlier_slot=0},
    {.crit_count_max=1, .minCritical=7, .maxCritical=13, .startingValue=10, .data=n0f0CriticalData[1], .crit_count=0, .outlier_present=0, .outlier_slot=0},
    {.crit_count_max=1, .minCritical=4, .maxCritical=11, .startingValue=7, .data=n0f0CriticalData[2], .crit_count=0, .outlier_present=0, .outlier_slot=0},
};

dataPoint n0f0DPs [7]={
    {.min=-100, .max=923, .bits=10, .minWarning=120, .maxWarning=923, .inWarningState=0, .criticalStructPtr=&n0f0CriticalDPs[0]},
    {.min=-100, .max=923, .bits=10, .minWarning=100, .maxWarning=923, .inWarningState=0, .criticalStructPtr=NULL},
    {.min=0, .max=2, .bits=2, .minWarning=0, .maxWarning=2, .inWarningState=0, .criticalStructPtr=NULL},
    {.min=0, .max=2, .bits=2, .minWarning=0, .maxWarning=2, .inWarningState=0, .criticalStructPtr=NULL},
    {.min=0, .max=255, .bits=8, .minWarning=8, .maxWarning=12, .inWarningState=0, .criticalStructPtr=&n0f0CriticalDPs[1]},
    {.min=0, .max=100, .bits=7, .minWarning=5, .maxWarning=10, .inWarningState=0, .criticalStructPtr=&n0f0CriticalDPs[2]},
    {.min=0, .max=1, .bits=1, .minWarning=0, .maxWarning=1, .inWarningState=0, .criticalStructPtr=NULL},
};

dataPoint n0f1DPs [2]={
    {.min=0, .max=1023, .bits=10, .minWarning=0, .maxWarning=1023, .inWarningState=0, .criticalStructPtr=NULL},
    {.min=37, .max=100, .bits=6, .minWarning=37, .maxWarning=100, .inWarningState=0, .criticalStructPtr=NULL},
};

CANFrame n0[2]={
    {.nodeIndex=0, .frameID=0, .numData=7, .dataTimeout=5000, .telemetryDivider=1, .hasCriticalData=1, .telemetryDivider_Count=0, .dataLocation=0, .consecutiveMisses=0, .dataInfo=n0f0DPs},
    {.nodeIndex=0, .frameID=1, .numData=2, .dataTimeout=15000, .telemetryDivider=1, .hasCriticalData=0, .telemetryDivider_Count=0, .dataLocation=0, .consecutiveMisses=0, .dataInfo=n0f1DPs},
};

// Node 1: testRuster
CANFrame n1[0]={
};

// Node 2: pedalSensor
dataPoint n2f0DPs [4]={
    {.min=-30000, .max=35000, .bits=16, .minWarning=4500, .maxWarning=6000, .inWarningState=0, .criticalStructPtr=NULL},
    {.min=-20, .max=120, .bits=8, .minWarning=-20, .maxWarning=120, .inWarningState=0, .criticalStructPtr=NULL},
    {.min=-20, .max=120, .bits=8, .minWarning=-20, .maxWarning=120, .inWarningState=0, .criticalStructPtr=NULL},
    {.min=-20, .max=120, .bits=8, .minWarning=-10, .maxWarning=110, .inWarningState=0, .criticalStructPtr=NULL},
};

CANFrame n2[1]={
    {.nodeIndex=2, .frameID=2, .numData=4, .dataTimeout=5000, .telemetryDivider=1, .hasCriticalData=0, .telemetryDivider_Count=0, .dataLocation=0, .consecutiveMisses=0, .dataInfo=n2f0DPs},
};

// Node 3: IMU
dataPoint n3f0DPs [4]={
    {.min=-100, .max=923, .bits=10, .minWarning=-100, .maxWarning=923, .inWarningState=0, .criticalStructPtr=NULL},
    {.min=-100, .max=1947, .bits=11, .minWarning=-100, .maxWarning=1947, .inWarningState=0, .criticalStructPtr=NULL},
    {.min=-100, .max=1947, .bits=11, .minWarning=-100, .maxWarning=1947, .inWarningState=0, .criticalStructPtr=NULL},
    {.min=-55, .max=200, .bits=8, .minWarning=0, .maxWarning=100, .inWarningState=0, .criticalStructPtr=NULL},
};

dataPoint n3f1DPs [3]={
    {.min=-1048576, .max=1048575, .bits=21, .minWarning=-1048576, .maxWarning=1048575, .inWarningState=0, .criticalStructPtr=NULL},
    {.min=-1048576, .max=1048575, .bits=21, .minWarning=-1048576, .maxWarning=1048575, .inWarningState=0, .criticalStructPtr=NULL},
    {.min=-1048576, .max=1048575, .bits=21, .minWarning=-1048576, .maxWarning=1048575, .inWarningState=0, .criticalStructPtr=NULL},
};

dataPoint n3f2DPs [3]={
    {.min=-32768, .max=32767, .bits=16, .minWarning=-32768, .maxWarning=32767, .inWarningState=0, .criticalStructPtr=NULL},
    {.min=-32768, .max=32767, .bits=16, .minWarning=-32768, .maxWarning=32767, .inWarningState=0, .criticalStructPtr=NULL},
    {.min=-32768, .max=32767, .bits=16, .minWarning=-32768, .maxWarning=32767, .inWarningState=0, .criticalStructPtr=NULL},
};

dataPoint n3f3DPs [6]={
    {.min=-180, .max=180, .bits=9, .minWarning=-180, .maxWarning=180, .inWarningState=0, .criticalStructPtr=NULL},
    {.min=-90, .max=90, .bits=8, .minWarning=-90, .maxWarning=90, .inWarningState=0, .criticalStructPtr=NULL},
    {.min=-180, .max=180, .bits=9, .minWarning=-180, .maxWarning=180, .inWarningState=0, .criticalStructPtr=NULL},
    {.min=-2048, .max=2047, .bits=12, .minWarning=-2048, .maxWarning=2047, .inWarningState=0, .criticalStructPtr=NULL},
    {.min=-2048, .max=2047, .bits=12, .minWarning=-2048, .maxWarning=2047, .inWarningState=0, .criticalStructPtr=NULL},
    {.min=-8192, .max=8191, .bits=14, .minWarning=-8192, .maxWarning=8191, .inWarningState=0, .criticalStructPtr=NULL},
};

CANFrame n3[4]={
    {.nodeIndex=3, .frameID=3, .numData=4, .dataTimeout=3000, .telemetryDivider=1, .hasCriticalData=0, .telemetryDivider_Count=0, .dataLocation=0, .consecutiveMisses=0, .dataInfo=n3f0DPs},
    {.nodeIndex=3, .frameID=4, .numData=3, .dataTimeout=3000, .telemetryDivider=1, .hasCriticalData=0, .telemetryDivider_Count=0, .dataLocation=0, .consecutiveMisses=0, .dataInfo=n3f1DPs},
    {.nodeIndex=3, .frameID=5, .numData=3, .dataTimeout=3000, .telemetryDivider=1, .hasCriticalData=0, .telemetryDivider_Count=0, .dataLocation=0, .consecutiveMisses=0, .dataInfo=n3f2DPs},
    {.nodeIndex=3, .frameID=6, .numData=6, .dataTimeout=3000, .telemetryDivider=1, .hasCriticalData=0, .telemetryDivider_Count=0, .dataLocation=0, .consecutiveMisses=0, .dataInfo=n3f3DPs},
};

// Node 4: powerDistribution
int32_t n4f0CriticalData[1][pointsPerData]={R8(26000)};

critical_dataPoint n4f0CriticalDPs [1]={
    {.crit_count_max=1, .minCritical=20000, .maxCritical=31000, .startingValue=26000, .data=n4f0CriticalData[0], .crit_count=0, .outlier_present=0, .outlier_slot=0},
};

dataPoint n4f0DPs [2]={
    {.min=-524288, .max=524287, .bits=20, .minWarning=22000, .maxWarning=29500, .inWarningState=0, .criticalStructPtr=&n4f0CriticalDPs[0]},
    {.min=-524288, .max=524287, .bits=20, .minWarning=-1, .maxWarning=25000, .inWarningState=0, .criticalStructPtr=NULL},
};

dataPoint n4f1DPs [5]={
    {.min=-32768, .max=32767, .bits=16, .minWarning=-32768, .maxWarning=32767, .inWarningState=0, .criticalStructPtr=NULL},
    {.min=-32768, .max=32767, .bits=16, .minWarning=-32768, .maxWarning=32767, .inWarningState=0, .criticalStructPtr=NULL},
    {.min=0, .max=255, .bits=8, .minWarning=0, .maxWarning=255, .inWarningState=0, .criticalStructPtr=NULL},
    {.min=0, .max=100, .bits=7, .minWarning=0, .maxWarning=100, .inWarningState=0, .criticalStructPtr=NULL},
    {.min=0, .max=1, .bits=1, .minWarning=0, .maxWarning=1, .inWarningState=0, .criticalStructPtr=NULL},
};

CANFrame n4[2]={
    {.nodeIndex=4, .frameID=7, .numData=2, .dataTimeout=3000, .telemetryDivider=1, .hasCriticalData=1, .telemetryDivider_Count=0, .dataLocation=0, .consecutiveMisses=0, .dataInfo=n4f0DPs},
    {.nodeIndex=4, .frameID=8, .numData=5, .dataTimeout=3000, .telemetryDivider=1, .hasCriticalData=0, .telemetryDivider_Count=0, .dataLocation=0, .consecutiveMisses=0, .dataInfo=n4f1DPs},
};

// vitalsData *nodes;
vitalsNode nodes [5]={
    {.CAN_ID=3, .flags=0, .milliSeconds=0, .numFrames=2, .CANFrames=n0},
    {.CAN_ID=7, .flags=0, .milliSeconds=0, .numFrames=0, .CANFrames=n1},
    {.CAN_ID=8, .flags=0, .milliSeconds=0, .numFrames=1, .CANFrames=n2},
    {.CAN_ID=9, .flags=0, .milliSeconds=0, .numFrames=4, .CANFrames=n3},
    {.CAN_ID=10, .flags=0, .milliSeconds=0, .numFrames=2, .CANFrames=n4},
};
