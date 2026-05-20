#include "vitalsPacketSendLUT.h"

// ----- HBTiming -----
const simpleDataPoint HBTiming_fields[7] = {
    { .bits=5, .min=0, .max=31 }, // Mask
    { .bits=7, .min=0, .max=127 },
    { .bits=10, .min=0, .max=1023 },
    { .bits=7, .min=0, .max=127 },
    { .bits=10, .min=0, .max=1023 },
    { .bits=7, .min=0, .max=127 },
    { .bits=10, .min=0, .max=1023 },
};

// ----- HBStatus -----
const simpleDataPoint HBStatus_fields[2] = {
    { .bits=3, .min=0, .max=7 }, // Mask
    { .bits=5, .min=0, .max=31 },
};

// ----- BusStatus -----
const simpleDataPoint BusStatus_fields[9] = {
    { .bits=4, .min=0, .max=15 }, // Mask
    { .bits=2, .min=0, .max=3 },
    { .bits=8, .min=-10, .max=245 },
    { .bits=8, .min=0, .max=255 },
    { .bits=12, .min=0, .max=4095 },
    { .bits=12, .min=0, .max=4095 },
    { .bits=11, .min=0, .max=2047 },
    { .bits=11, .min=0, .max=2047 },
    { .bits=4, .min=0, .max=15 },
};

// ----- vitalsErr -----
const simpleDataPoint vitalsErr_fields[2] = {
    { .bits=8, .min=0, .max=255 }, // Mask
    { .bits=8, .min=0, .max=255 },
};

// ----- dataWarning -----
const simpleDataPoint dataWarning_fields[7] = {
    { .bits=6, .min=0, .max=63 }, // Mask
    { .bits=1, .min=0, .max=1 },
    { .bits=1, .min=0, .max=1 },
    { .bits=3, .min=0, .max=4 },
    { .bits=7, .min=0, .max=127 },
    { .bits=2, .min=0, .max=3 },
    { .bits=4, .min=0, .max=15 },
};

// ----- nodeStatus -----
const simpleDataPoint nodeStatus_fields[3] = {
    { .bits=7, .min=0, .max=127 }, // Mask
    { .bits=7, .min=0, .max=127 },
    { .bits=2, .min=0, .max=3 },
};

// ----- unknownCanPacket -----
const simpleDataPoint unknownCanPacket_fields[1] = {
    { .bits=8, .min=0, .max=255 }, // Mask
};

// ----- CANDataFrame -----
const simpleDataPoint CANDataFrame_fields[2] = {
    { .bits=1, .min=0, .max=1 }, // Mask
    { .bits=7, .min=0, .max=127 },
};

