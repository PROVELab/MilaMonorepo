#include "myDefines.hpp"
#include "../common/sensorHelper.hpp"

//creates CANFrame array from this node. It stores data to be sent, and info for how to send

dataPoint f0DataPoints [5]={
    {.min=-100, .max=923, .enum=None, .bits=10},
    {.min=-100, .max=923, .enum=None, .bits=10},
    {.min=0, .max=2, .enum=prechargeState, .bits=2},
    {.min=0, .max=2, .enum=prechargeState, .bits=2},
    {.min=0, .max=1, .enum=None, .bits=1},
};

CANFrame myframes[numFrames] = {
    {.numData = 5, .frequency = 100, .startingDataIndex=0, .dataInfo=f0DataPoints},
};
