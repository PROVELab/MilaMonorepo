#include <stdbool.h> // For bool type
#ifndef SENSOR_HELP
#define SENSOR_HELP

#ifdef __cplusplus
extern "C" { //Need C linkage since ESP uses C "C"
#endif
#include "../../programConstants.h"
#include "../../pecan/pecan.h"
#include <stdint.h>
#include <stddef.h> // For size_t

// This needs to be included before its macros are used by other declarations
#define STRINGIZE_(a) #a
#define STRINGIZE(a) STRINGIZE_(a)
#include STRINGIZE(NODE_CONFIG)  //includes node Constants

//universal globals. Used by every sensor
typedef struct { //identified by a 2 bit identifier 0-3 in function code
    int8_t numData;
    int32_t period;
    int8_t startingDataIndex;  //starting index of data in this frame. used by collector function
    simpleDataPoint *dataInfo;
} CANFrame;

extern CANFrame myframes[numFrames];

//For ts, pass PScheduler* for arduino, else pass NULL
int8_t sensorInit(PCANListenParamsCollection* plpc, void* ts);
void sendFrame(int8_t frameNum);

#ifdef SENSOR_HAS_COMMANDS
// Struct for command lookup table entries
typedef struct SensorRecvPacketLUTEntry_s {
    const simpleDataPoint* fields;
    uint8_t num_fields;
    bool packetIsCustom;
    void (*callback_wrapper)(const uint8_t* raw_packet, size_t packet_len, int8_t* bitIndex);
} SensorRecvPacketLUTEntry;

void registerCommandHandler(PCANListenParamsCollection* plpc);

// Extern declarations for the command lookup table, defined in sensorRecvLUT.cpp
extern const SensorRecvPacketLUTEntry sensorRecvPacketLUT[];
extern const size_t sensorRecvPacketLUTSize;
#endif

#ifdef __cplusplus
}  // End extern "C"
#endif
#endif
