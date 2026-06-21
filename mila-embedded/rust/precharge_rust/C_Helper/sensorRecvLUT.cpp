#include "myDefines.hpp"
#include <string.h>
 #include "../../../src/sensors/common/sensorHelper.hpp"
#include "../../../src/pecan/pecan.h"

extern "C" {

const simpleDataPoint setChargeCondition_fields[2] = {
    { 80, 335, 8 },
    { 68, 99, 5 },
};

static void setChargeCondition_wrapper(const uint8_t* raw_packet, size_t packet_len, int8_t* bitIndex) {
    union {
        setChargeCondition_args_t s;
        int32_t data_arr[2];
    } u __attribute__((aligned(4)));

    uint8_t unpack_packet[8] = {0};
    if (packet_len > sizeof(unpack_packet)) { return; }
    memcpy(unpack_packet, raw_packet, packet_len);
    for (int i = 0; i < 2; ++i) {
        pecan_unpack(&u.data_arr[i], &unpack_packet, &setChargeCondition_fields[i], bitIndex);
    }
    onsetChargeCondition(u.s);
}

const SensorRecvPacketLUTEntry sensorRecvPacketLUT[] = {
    { // setChargeCondition
        .fields = setChargeCondition_fields,
        .num_fields = 2,
        .packetIsCustom = false,
        .callback_wrapper = setChargeCondition_wrapper,
    },
};
const size_t sensorRecvPacketLUTSize = sizeof(sensorRecvPacketLUT) / sizeof(SensorRecvPacketLUTEntry);

} // extern C
