#include "pecan/pecan.h" // For simpleDataPoint
#include "myDefines.hpp"
#include "../../../src/sensors/common/sensorHelper.hpp"

#ifdef __cplusplus
extern "C" {
#endif
//creates CANFrame array from this node. It stores data to be sent, and info for how to send

CANFrame myframes[numFrames] = {
};

#ifdef __cplusplus
}
#endif
