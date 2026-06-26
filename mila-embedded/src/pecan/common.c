#include "../programConstants.h"
#include "pecan.h"
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h> // memcpy
// #include <memory>

// #include <type_traits>
#include <assert.h>

uint32_t combinedID(uint32_t fn_id, uint32_t node_id) { return (fn_id << 7) + node_id; }
uint32_t combinedIDExtended(uint32_t fn_id, uint32_t node_id, uint32_t extension) {
    return combinedID(fn_id, node_id) + (extension << 11);
}
uint32_t getNodeId(uint32_t id) { return id & 0b1111111; }
uint32_t getFunctionId(uint32_t id) { return (id >> 7) & 0b1111; }
uint32_t getIdExtension(uint32_t id) { return (id >> 11) & 0b111111111111111111; }
uint32_t getDataFrameId(uint32_t id) {
    return getIdExtension(id) & ((0b1 << maxFrameCntBits) - 1);
}

//helper for default print
static void printHex(uint8_t value) {
    static const char hex_digits[] = "0123456789ABCDEF";
    char buf[4] = {
        hex_digits[(value >> 4) & 0x0F],
        hex_digits[value & 0x0F],
        ' ',
        '\0',
    };
    flexiblePrint(buf);
}

int16_t defaultPacketRecv(CANPacket* p) {
    // Print the id
    flexiblePrint("Default recv: id 0x");
    for(int i = 0; i < 4; i++){
        printHex(((uint8_t*)p->id)[i]);
    }

    // Print each data element
    flexiblePrint("\n with data: 0x");
    if (p->rtr == false) {
        for (int i = 0; i < p->dataSize; i++) {
            printHex(p->data[i]);
        }
    }

    // Print the final newline
    flexiblePrint("\n");
    return 0;
}

int16_t addParam(PCANListenParamsCollection* plpc, CANListenParam clp) {
    if (plpc->size + 1 > MAX_PCAN_PARAMS) {
        return NOSPACE;
    } else {
        plpc->arr[plpc->size] = clp;
        plpc->size++;
        return SUCCESS;
    }
}
int16_t setRTR(CANPacket* p) { // makes the given packet an RTR packet
    if (p->dataSize != 0) {
        return 1; // this packet has data written to it, it cant also be an rtr packet
    }
    p->rtr = 1;
    return 0;
}
int16_t setExtended(CANPacket* p) { // makes the given packet an extended ID packet (so can send 29 bits of id, instead
                                    // of just first 11)
    p->extendedID = 1;
    return 0;
}

int16_t writeData(CANPacket* p, int8_t* dataPoint, int16_t size) {
    if (p->rtr) {
        return -4; // this is an rtr packet, can not write data
    }
    const size_t originalSize = p->dataSize;
    if (originalSize + size > MAX_SIZE_PACKET_DATA) { return NOSPACE; }
    memcpy(&(p->data[originalSize]), dataPoint, size);
    (p->dataSize) += size;
    return SUCCESS;
}

void pecan_unpack(int32_t *const restrict dest, const uint8_t (*const restrict src)[8],
                  const simpleDataPoint* const restrict field_info, int8_t* const restrict bitIndex)
{
    uint32_t temp = 0;
    copyDataToValue(&temp, src, *bitIndex, field_info->bits);
    *dest = ((int32_t)temp) + field_info->min;
    *bitIndex += field_info->bits;
}

void pecan_pack(uint8_t (*const restrict dest_buffer)[8], int8_t* const restrict bit_index,
                const int32_t value, const simpleDataPoint* const restrict field_info)
{
    uint32_t formatted_val = formatValue(value, field_info->min, field_info->max);
    copyValueToData(&formatted_val, dest_buffer, *bit_index, field_info->bits);
    *bit_index += field_info->bits;
}

// copies the first numBits of value into target starting at startBit of target
// requirement: it ors into the target, (does not reset already set bits). value should fit within numBits
int16_t copyValueToData(const uint32_t *const restrict value, uint8_t (*const restrict target)[8],
                        const int8_t startBit, const int8_t numBits)
{ 

    // To avoid strict aliasing violations
    uint64_t target_val;
    memcpy(&target_val, target, sizeof(target_val));
    uint64_t source_val = ((uint64_t)(*value)) & ((1ULL << numBits) - 1);
    target_val |= (source_val << startBit);

    memcpy(target, &target_val, sizeof(target_val));
    return 0;
}

// inverse of the above function. requires mc to be little endian
int16_t copyDataToValue(uint32_t *const restrict target, const uint8_t (*const restrict data)[8],
                        const int8_t startBit, const int8_t numBits)
{
    uint64_t data_val;
    memcpy(&data_val, data, sizeof(data_val));

    const uint64_t mask = (1ULL << numBits) - 1;
    *target = (uint32_t)((data_val >> startBit) & mask);
    return 0;
}

// returns value constrained to min of min, and max of max
int32_t squeeze(const int32_t value, const int32_t min, const int32_t max) {
    return (value < min) ? min : (value > max ? max : value);
}
//squeeze and subtract min, forces the value to be unsigned and in bounds
uint32_t formatValue(const int32_t value, const int32_t min, const int32_t max) {
    return (uint32_t) (squeeze(value, min, max) - min);
}


void sendStatusUpdate(uint8_t flag, uint32_t Id) {
    CANPacket statusUpdatePacket;
    memset(&statusUpdatePacket, 0, sizeof(CANPacket));
    statusUpdatePacket.id = combinedID(statusUpdate, Id);
    writeData(&statusUpdatePacket, (int8_t*) &flag, 1);
    sendPacket(&statusUpdatePacket);
}

bool exact(uint32_t id, uint32_t mask) { // does not check extended bits of Id
    return (id & 0b11111111111) == mask;
}

// note: ID sent over CAN is 11 bit long, with first 7 bitsbeing the identifier of sending node, and last 4 bits being
// the function code
bool matchID(uint32_t id, uint32_t mask) { // check if the 7 bits of node ID must match mask
    return getNodeId(id) == getNodeId(mask);
}

bool matchFunction(uint32_t id, uint32_t mask) {     // mask should contain 4 bit functoin code in bits 7-10.
    return getFunctionId(id) == getFunctionId(mask); // only compares the functionCodes
}

#include <assert.h>
#include <stdint.h>

static_assert(
    _Generic((uint8_t)0, 
        unsigned char: 1, 
        char: 1, 
        default: 0
    ), 
    "pecan uses uint8_t to alias other types. " 
    "If its not a unsigned char or char, this is UB"
);

// static_assert(std::is_same_v<uint8_t, unsigned char> || 
//               std::is_same_v<uint8_t, char>, 
//               "pecan uses uint8_t to alias other types. " 
//               "If its not a unsigned char or chart, this is UB");
