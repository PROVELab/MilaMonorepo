#ifndef pedalSensor_DATA_H
#define pedalSensor_DATA_H
//defines constants specific to pedalSensor
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h> // For size_t
#define myId 8
#define numFrames 1
#define node_numData 5

int32_t collect_pedalPowerReadingmV(bool* cancelFrameSend);
int32_t collect_pedalReadingOne(bool* cancelFrameSend);
int32_t collect_pedalReadingTwo(bool* cancelFrameSend);
int32_t collect_brakeReading(bool* cancelFrameSend);
int32_t collect_SDIO_det_pin(bool* cancelFrameSend);

#define dataCollectorsList collect_pedalPowerReadingmV, collect_pedalReadingOne, collect_pedalReadingTwo, collect_brakeReading, collect_SDIO_det_pin

#endif