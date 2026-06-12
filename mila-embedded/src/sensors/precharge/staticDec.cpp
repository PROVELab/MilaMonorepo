#include "pecan/pecan.h" // For simpleDataPoint
#include "myDefines.hpp"
#include "../common/sensorHelper.hpp"

#ifdef __cplusplus
extern "C" {
#endif
//creates CANFrame array from this node. It stores data to be sent, and info for how to send

simpleDataPoint f0DataPoints [5]={
    { .min=-100, .max=923, .bits=10 },
    { .min=-100, .max=923, .bits=10 },
    { .min=0, .max=2, .bits=2 },
    { .min=0, .max=2, .bits=2 },
    { .min=0, .max=1, .bits=1 },
};

CANFrame myframes[numFrames] = {
    {.numData = 5, .frequency = 100, .startingDataIndex=0, .dataInfo=f0DataPoints},
};

#ifdef __cplusplus
}
#endif
