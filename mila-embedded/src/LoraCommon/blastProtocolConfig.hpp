#pragma once
#include "Driver/Config.hpp"   //maxLoraPacketSize, driverPacket
#include <stddef.h>
//TX burst and queue configuration
#define maxPacketGroupSize 16
//

//move to auto-generated file at some point
#define vitalsErr 0x01 

//packet formats
//header types
typedef uint16_t protocolID_t;   //2 byte protocol ID
#define protocolUniqueID 0x9354


typedef uint8_t tx_frameTrack_t; //4 bits frame num, 4 bits #frames in this burst. 16 frames max in burst
namespace FrameTrack {
    constexpr uint8_t frameNumMask  = 0b00001111;
    constexpr uint8_t burstSizeMask = 0b11110000;

    constexpr void set_frameNum(tx_frameTrack_t& input, uint8_t frameNum) {
        input &= ~frameNumMask;
        input |= (frameNum & frameNumMask);
    }
    constexpr void set_burstSize(tx_frameTrack_t& input, uint8_t burstSize) {
        input &= ~burstSizeMask;
        input |= ((burstSize & 0x0F) << 4);
    }

    constexpr uint8_t get_frameNum(tx_frameTrack_t input) {
        return input & frameNumMask; //lower 4 bits
    }

    constexpr uint8_t get_burstSize(tx_frameTrack_t input) {
        return (input & burstSizeMask) >> 4; //upper 4 bits
    }
}

typedef uint8_t tx_flags_t;
typedef uint8_t rx_flags_t;
typedef uint16_t rx_bitmap_t;   //needs to have at least as many bits as maxPacketGroupSize
static_assert(sizeof(rx_bitmap_t)*8 >= maxPacketGroupSize);
//

//format for packets sent by TX side of protocol
#define TXHeaderSize ( sizeof(protocolID_t) + sizeof(tx_frameTrack_t) + sizeof(tx_flags_t) )
#define maxTXDataSize (maxLoraPacketSize - TXHeaderSize)
struct __attribute__((packed)) TXProtocolPacket {
    protocolID_t protocolID;
    tx_flags_t flags;
    tx_frameTrack_t frameNum;
    uint8_t data[maxTXDataSize];
};
//format for packets sent by RX side of protocol
#define RXHeaderSize (sizeof(protocolID_t) + sizeof(rx_flags_t) + sizeof(rx_bitmap_t))
#define maxRXDataSize (maxLoraPacketSize - RXHeaderSize)
struct __attribute__((packed)) RXProtocolPacket {
    protocolID_t protocolID;
    rx_flags_t flags;
    rx_bitmap_t bitmap;
    uint8_t data[maxRXDataSize];
};
//
// The static asserts below are no longer valid or necessary with this cleaner approach.

namespace rxFlagMasks {
    enum : rx_flags_t {
        ackParityMask = 1,
    };
}

namespace txFlagMasks {
    enum : tx_flags_t {
        ackParityMask = 1,
        errorPacketMask = 2,
        priorityPacketMask = 4,
        firstBurstMask = 8
    };
}
