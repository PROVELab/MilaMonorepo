#include "myDefines.hpp"
#include "../common/sensorHelper.hpp"

//creates CANFrame array from this node. It stores data to be sent, and info for how to send

dataPoint f0DataPoints [4]={
    {.min=-100, .max=923, .enum=None, .bits=10},
    {.min=-100, .max=923, .enum=None, .bits=10},
    {.min=-100, .max=923, .enum=None, .bits=10},
    {.min=-55, .max=200, .enum=None, .bits=8},
};

dataPoint f2DataPoints [3]={
    {.min=-1048576, .max=1048575, .enum=None, .bits=21},
    {.min=-1048576, .max=1048575, .enum=None, .bits=21},
    {.min=-1048576, .max=1048575, .enum=None, .bits=21},
};

dataPoint f4DataPoints [5]={
    {.min=-32768, .max=32767, .enum=None, .bits=16},
    {.min=-32768, .max=32767, .enum=None, .bits=16},
    {.min=-128, .max=127, .enum=None, .bits=8},
    {.min=-128, .max=127, .enum=None, .bits=8},
    {.min=-128, .max=127, .enum=None, .bits=8},
};

CANFrame myframes[numFrames] = {
    {.numData = 4, .frequency = 100, .startingDataIndex=0, .dataInfo=f0DataPoints},
    {.numData = 3, .frequency = 1000, .startingDataIndex=4, .dataInfo=f1DataPoints},
    {.numData = 5, .frequency = 1000, .startingDataIndex=7, .dataInfo=f2DataPoints},
};
