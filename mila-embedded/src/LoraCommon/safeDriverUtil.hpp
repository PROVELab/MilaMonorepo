

#include <stdint.h>
#include <stddef.h>
#include "../LoraCommon/Driver/Driver.hpp"

typedef enum {
    Success,
    Timeout,
    Crashed,
    Unknown
} result;

result safeLoraTx(const driverSendPacket* packet, const uint64_t timerExpireTime_us);

result safeWaitForRecv(driverRecvPacket*& packet, const uint64_t timerExpireTime_us);
