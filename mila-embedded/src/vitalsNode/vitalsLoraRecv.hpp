
#ifndef VITALS_LORA_RECV_HPP
#define VITALS_LORA_RECV_HPP

#include "../pecan/pecan.h"

#ifdef __cplusplus
extern "C" {
#endif

void loraRecvTask(void* pvParameters);

//used internally. called by wrappers to forward a packet
void forwardCANPacket(uint32_t target_node_id, uint32_t can_mask, uint8_t can_mask_bits, const simpleDataPoint* fields, uint8_t num_fields, uint8_t packet_type, const int32_t* data_arr, const uint8_t* raw_packet, size_t packet_len, int8_t* bitIndex);


#ifdef __cplusplus
}
#endif

#endif // VITALS_LORA_HPP
