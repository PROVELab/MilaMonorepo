#include "pecan/pecan.h" // For simpleDataPoint
#include "myDefines.hpp"
#include "../../../src/sensors/common/sensorHelper.hpp"

#ifdef __cplusplus
extern "C" {
#endif
//creates CANFrame array from this node. It stores data to be sent, and info for how to send

simpleDataPoint f0DataPoints [7]={
    { .min=-100, .max=923, .bits=10 },
    { .min=-100, .max=923, .bits=10 },
    { .min=0, .max=2, .bits=2 },
    { .min=0, .max=2, .bits=2 },
    { .min=0, .max=255, .bits=8 },
    { .min=0, .max=100, .bits=7 },
    { .min=0, .max=1, .bits=1 },
};

simpleDataPoint f1DataPoints [2]={
    { .min=0, .max=1023, .bits=10 },
    { .min=37, .max=100, .bits=6 },
};

CANFrame myframes[numFrames] = {
    {.numData = 7, .period = 500, .startingDataIndex=0, .dataInfo=f0DataPoints},
    {.numData = 2, .period = 5000, .startingDataIndex=7, .dataInfo=f1DataPoints},
};

#ifdef __cplusplus
}
#endif
