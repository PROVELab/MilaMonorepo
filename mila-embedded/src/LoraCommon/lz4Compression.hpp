#pragma once

#include <stdint.h>
#include "blastProtocolConfig.hpp"

#define LZ4_ACCEL 1                  // Higher = faster but worse compression
#define LZ4_16KB_WORKSPACE 16384     // Standard LZ4 hash table size
#define LZ4_MIN_DATA_SIZE 8          // Avoid magic numbers & can change this later

// Returns true if compression succeeded and packet was compressed.
// Modifies packet in-place and updates flags accordingly.
// Call once per packet after popping from queue, before transmitting.
bool compressPacket(driverSendPacket* packet);

// Returns true if decompression succeeded.
// Modifies packet in-place, replaces compressed payload with raw payload.
// Call on RX side after validating header and checking compressedPacketMask.
bool decompressPacket(driverSendPacket* packet);
