#ifndef TEMP_HUMID_H
#define TEMP_HUMID_H

#ifdef __cplusplus
extern "C" { 
#endif

#include <stdint.h>

void initTempHumid();

int32_t getTempF();

int32_t getRH();

#ifdef __cplusplus
} // End extern "C"
#endif

#endif //TEMP_HUMID_H