#include "pecan/pecan.h" // For simpleDataPoint
#include "myDefines.hpp"
#include "../common/sensorHelper.hpp"

#ifdef __cplusplus
extern "C" {
#endif
//creates CANFrame array from this node. It stores data to be sent, and info for how to send

simpleDataPoint f0DataPoints [2]={
    { .min=-22768, .max=42767, .bits=16 },
    { .min=-32768, .max=32767, .bits=16 },
};

simpleDataPoint f1DataPoints [5]={
    { .min=-32768, .max=32767, .bits=16 },
    { .min=-32768, .max=32767, .bits=16 },
    { .min=0, .max=255, .bits=8 },
    { .min=0, .max=100, .bits=7 },
    { .min=0, .max=1, .bits=1 },
};

CANFrame myframes[numFrames] = {
    {.numData = 2, .frequency = 100, .startingDataIndex=0, .dataInfo=f0DataPoints},
    {.numData = 5, .frequency = 200, .startingDataIndex=2, .dataInfo=f1DataPoints},
};

#ifdef __cplusplus
}
#endif
