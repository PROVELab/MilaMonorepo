#include "myDefines.hpp"
#include "../common/sensorHelper.hpp"

//creates CANFrame array from this node. It stores data to be sent, and info for how to send

dataPoint f0DataPoints [5]={
    {.min=-30000, .max=35000, .enum=None, .bits=16},
    {.min=-20, .max=120, .enum=None, .bits=8},
    {.min=-20, .max=120, .enum=None, .bits=8},
    {.min=-20, .max=120, .enum=None, .bits=8},
    {.min=0, .max=1, .enum=None, .bits=1},
};

CANFrame myframes[numFrames] = {
    {.numData = 5, .frequency = 1000, .startingDataIndex=0, .dataInfo=f0DataPoints},
};
