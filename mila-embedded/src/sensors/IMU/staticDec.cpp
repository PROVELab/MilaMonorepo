#include "pecan/pecan.h" // For simpleDataPoint
#include "myDefines.hpp"
#include "../common/sensorHelper.hpp"

#ifdef __cplusplus
extern "C" {
#endif
//creates CANFrame array from this node. It stores data to be sent, and info for how to send

simpleDataPoint f0DataPoints [4]={
    { .min=-100, .max=923, .bits=10 },
    { .min=-100, .max=923, .bits=10 },
    { .min=-100, .max=923, .bits=10 },
    { .min=-55, .max=200, .bits=8 },
};

simpleDataPoint f1DataPoints [3]={
    { .min=-1048576, .max=1048575, .bits=21 },
    { .min=-1048576, .max=1048575, .bits=21 },
    { .min=-1048576, .max=1048575, .bits=21 },
};

simpleDataPoint f2DataPoints [5]={
    { .min=-32768, .max=32767, .bits=16 },
    { .min=-32768, .max=32767, .bits=16 },
    { .min=-128, .max=127, .bits=8 },
    { .min=-128, .max=127, .bits=8 },
    { .min=-128, .max=127, .bits=8 },
};

CANFrame myframes[numFrames] = {
    {.numData = 4, .frequency = 100, .startingDataIndex=0, .dataInfo=f0DataPoints},
    {.numData = 3, .frequency = 1000, .startingDataIndex=4, .dataInfo=f1DataPoints},
    {.numData = 5, .frequency = 1000, .startingDataIndex=7, .dataInfo=f2DataPoints},
};

#ifdef __cplusplus
}
#endif
