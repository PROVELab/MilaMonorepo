#ifndef VITALS_SEND_DATA_H
#define VITALS_SEND_DATA_H

#include <stdint.h>
#include <stddef.h>
#include "../../pecan/pecan.h"

#ifdef __cplusplus
extern "C" {
#endif

//sending
uint8_t formatPacketCore(const simpleDataPoint* fields, size_t numFields, const int32_t* data, uint8_t* tempData);
uint8_t formatPacketVariable(const simpleDataPoint* fields, size_t numFields, const int32_t* data, const uint8_t* payload, size_t payloadBytes, uint8_t* outBuffer);

void sendPacketVariable(const simpleDataPoint* fields, size_t numFields, const int32_t* data, const uint8_t* payload, size_t payloadBytes);
void sendPacketCore(const simpleDataPoint* fields, size_t numFields, const int32_t* data, uint8_t* dataBuffer);

//automatic forwarding to CAN for applicable Lora packets
void forwardLoraToCAN(uint32_t target_node_id, uint32_t can_mask, uint8_t can_mask_bits, const simpleDataPoint* fields, uint8_t num_fields, uint8_t packet_type, const int32_t* data_arr, const uint8_t* raw_packet, size_t packet_len, int32_t* bitIndex);

void unpackDataStream(int32_t* target, const int numValues, const uint8_t* data, const simpleDataPoint* dataPoints, int32_t* totalBits);


//to access the look up tables
typedef struct {
    const simpleDataPoint* fields;
    uint8_t num_fields;
    uint8_t packet_type; // RECV_PACKET_TYPE_FIXED or RECV_PACKET_TYPE_CUSTOM
    uint32_t mask_val;
    uint8_t mask_bits;
    size_t (*callback_wrapper)(const uint8_t* raw_packet, size_t packet_len, int32_t* initial_bitIndex);
} RecvPacketLUTEntry;

extern const uint8_t MAX_RECV_MASK_BITS;
extern const RecvPacketLUTEntry recvPacketLUT[];
extern const size_t recvPacketLUTSize;


#ifdef __cplusplus
}
#endif

#endif
