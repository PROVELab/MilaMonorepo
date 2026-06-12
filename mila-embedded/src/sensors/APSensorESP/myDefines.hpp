#ifndef APSensorESP_DATA_H
#define APSensorESP_DATA_H
//defines constants specific to APSensorESP
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h> // For size_t
#define myId 11
#define numFrames 1
#define node_numData 1

int32_t collect_airPressure(bool* cancelFrameSend);

#define dataCollectorsList collect_airPressure

#endif