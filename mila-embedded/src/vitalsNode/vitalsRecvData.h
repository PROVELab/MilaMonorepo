#ifndef VITALS_RECV_DATA_H
#define VITALS_RECV_DATA_H

#include <stdint.h>
#include <stddef.h>
#include "pecan/pecan.h" // For simpleDataPoint

#ifdef __cplusplus
extern "C" {
#endif

void processReceivedData(uint8_t* data, size_t len);

void forwardCANPacket(
    uint32_t targetNodeId,
    uint32_t can_mask_val,
    uint8_t can_mask_bits,
    const simpleDataPoint* fields,
    uint8_t num_fields,
    uint8_t packet_type,
    const void* args_ptr,
    const uint8_t* raw_telem_packet,
    size_t telem_packet_len,
    int8_t* telem_bit_idx_ptr
);

#ifdef __cplusplus
}
#endif

#endif // VITALS_RECV_DATA_H