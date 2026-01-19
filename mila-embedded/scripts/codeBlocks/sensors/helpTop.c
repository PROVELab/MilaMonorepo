#ifndef SENSOR_HELP
#define SENSOR_HELP

#ifdef __cplusplus
extern "C" { //Need C linkage since ESP uses C "C"
#endif
#include "../../programConstants.h"
#define STRINGIZE_(a) #a
#define STRINGIZE(a) STRINGIZE_(a)
#include STRINGIZE(../NODE_CONFIG)  //includes node Constants

#include "../../pecan/pecan.h"
#include <stdint.h>

//universal globals. Used by every sensor
