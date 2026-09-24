#include "pecan/pecan.h" // For simpleDataPoint
#include "myDefines.hpp"
#include "../../common/sensorHelper.hpp"

#ifdef __cplusplus
extern "C" {
#endif
//creates CANFrame array from this node. It stores data to be sent, and info for how to send

simpleDataPoint f0DataPoints [4]={
    { .min=-30000, .max=35000, .bits=16 },
    { .min=-20, .max=120, .bits=8 },
    { .min=-20, .max=120, .bits=8 },
    { .min=-20, .max=120, .bits=8 },
};

CANFrame myframes[numFrames] = {
    {.numData = 4, .period = 10, .startingDataIndex=0, .dataInfo=f0DataPoints},
};

#ifdef __cplusplus
}
#endif
