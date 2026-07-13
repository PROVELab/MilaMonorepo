#include "pecan/pecan.h" // For simpleDataPoint
#include "myDefines.hpp"
#include "../../common/sensorHelper.hpp"

#ifdef __cplusplus
extern "C" {
#endif
//creates CANFrame array from this node. It stores data to be sent, and info for how to send

simpleDataPoint f0DataPoints [4]={
    { .min=-100, .max=923, .bits=10 },
    { .min=-100, .max=1947, .bits=11 },
    { .min=-100, .max=1947, .bits=11 },
    { .min=-55, .max=200, .bits=8 },
};

simpleDataPoint f1DataPoints [3]={
    { .min=-1048576, .max=1048575, .bits=21 },
    { .min=-1048576, .max=1048575, .bits=21 },
    { .min=-1048576, .max=1048575, .bits=21 },
};

simpleDataPoint f2DataPoints [3]={
    { .min=-32768, .max=32767, .bits=16 },
    { .min=-32768, .max=32767, .bits=16 },
    { .min=-32768, .max=32767, .bits=16 },
};

simpleDataPoint f3DataPoints [6]={
    { .min=-180, .max=180, .bits=9 },
    { .min=-90, .max=90, .bits=8 },
    { .min=-180, .max=180, .bits=9 },
    { .min=-2048, .max=2047, .bits=12 },
    { .min=-2048, .max=2047, .bits=12 },
    { .min=-8192, .max=8191, .bits=14 },
};

CANFrame myframes[numFrames] = {
    {.numData = 4, .period = 800, .startingDataIndex=0, .dataInfo=f0DataPoints},
    {.numData = 3, .period = 800, .startingDataIndex=4, .dataInfo=f1DataPoints},
    {.numData = 3, .period = 800, .startingDataIndex=7, .dataInfo=f2DataPoints},
    {.numData = 6, .period = 800, .startingDataIndex=10, .dataInfo=f3DataPoints},
};

#ifdef __cplusplus
}
#endif
