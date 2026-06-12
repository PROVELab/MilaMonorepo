#include "../myDefines.h"
#include <string.h>

extern "C" {

const simpleDataPoint intermoduleCommand_fields[1] = {
    { 0, 2, 2 },
};

static void intermoduleCommand_wrapper(const uint8_t* raw_packet, size_t packet_len, int8_t* bitIndex) {
    union {
        intermoduleCommand_args_t s;
        int32_t data_arr[1];
    } u __attribute__((aligned(4)));

    for (int i = 0; i < 1; ++i) {
        pecan_unpack(&u.data_arr[i], raw_packet, &intermoduleCommand_fields[i], bitIndex);
    }
    onintermoduleCommand(u.s);
}

const SensorRecvPacketLUTEntry sensorRecvPacketLUT[] = {
    { // intermoduleCommand
        .fields = intermoduleCommand_fields,
        .num_fields = 1,
        .packetIsCustom = false,
        .callback_wrapper = intermoduleCommand_wrapper,
    },
};
const size_t sensorRecvPacketLUTSize = sizeof(sensorRecvPacketLUT) / sizeof(SensorRecvPacketLUTEntry);

} // extern C
