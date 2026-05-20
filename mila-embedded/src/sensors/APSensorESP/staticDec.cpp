#include "pecan/pecan.h" // For simpleDataPoint
#include "myDefines.hpp"
#include "../common/sensorHelper.hpp"

#ifdef __cplusplus
extern "C" {
#endif
//creates CANFrame array from this node. It stores data to be sent, and info for how to send

simpleDataPoint f0DataPoints [1]={
    { .min=-2147483648, .max=2147483647, .bits=32 },
};

CANFrame myframes[numFrames] = {
    {.numData = 1, .frequency = 700, .startingDataIndex=0, .dataInfo=f0DataPoints},
};

#ifdef __cplusplus
}
#endif
