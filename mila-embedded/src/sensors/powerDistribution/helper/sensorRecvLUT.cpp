#include "myDefines.hpp"
#include <string.h>
 #include "../../common/sensorHelper.hpp"
#include "../../../pecan/pecan.h"

extern "C" {

const simpleDataPoint setCoolantDutyCycle_fields[1] = {
    { 0, 100, 7 },
};

const simpleDataPoint setCoolantFrequency_HZ_fields[1] = {
    { 0, 65535, 16 },
};

static void setCoolantDutyCycle_wrapper(const uint8_t* raw_packet, size_t packet_len, int8_t* bitIndex) {
    union {
        setCoolantDutyCycle_args_t s;
        int32_t data_arr[1];
    } u __attribute__((aligned(4)));

    uint8_t unpack_packet[8] = {0};
    if (packet_len > sizeof(unpack_packet)) { return; }
    memcpy(unpack_packet, raw_packet, packet_len);
    for (int i = 0; i < 1; ++i) {
        pecan_unpack(&u.data_arr[i], &unpack_packet, &setCoolantDutyCycle_fields[i], bitIndex);
    }
    onsetCoolantDutyCycle(u.s);
}

static void setCoolantFrequency_HZ_wrapper(const uint8_t* raw_packet, size_t packet_len, int8_t* bitIndex) {
    union {
        setCoolantFrequency_HZ_args_t s;
        int32_t data_arr[1];
    } u __attribute__((aligned(4)));

    uint8_t unpack_packet[8] = {0};
    if (packet_len > sizeof(unpack_packet)) { return; }
    memcpy(unpack_packet, raw_packet, packet_len);
    for (int i = 0; i < 1; ++i) {
        pecan_unpack(&u.data_arr[i], &unpack_packet, &setCoolantFrequency_HZ_fields[i], bitIndex);
    }
    onsetCoolantFrequency_HZ(u.s);
}

const SensorRecvPacketLUTEntry sensorRecvPacketLUT[] = {
    { // setCoolantDutyCycle
        .fields = setCoolantDutyCycle_fields,
        .num_fields = 1,
        .packetIsCustom = false,
        .callback_wrapper = setCoolantDutyCycle_wrapper,
    },
    { // setCoolantFrequency_HZ
        .fields = setCoolantFrequency_HZ_fields,
        .num_fields = 1,
        .packetIsCustom = false,
        .callback_wrapper = setCoolantFrequency_HZ_wrapper,
    },
};
const size_t sensorRecvPacketLUTSize = sizeof(sensorRecvPacketLUT) / sizeof(SensorRecvPacketLUTEntry);

} // extern C
