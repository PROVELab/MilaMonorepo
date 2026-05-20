
#include <string.h>
#include "lz4Compression.hpp"
#include "lz4.h"
#include "blastProtocolConfig.hpp"
#include "esp_log.h"

static const char* TAG = "LZ4";

static char lz4Workspace[LZ4_16KB_WORKSPACE]; 
static uint8_t compressedBuffer[LZ4_COMPRESSBOUND(255)]; // Worst case = 255 bytes (max Lora packet size)


bool compressPacket(driverSendPacket* packet) {
    const uint8_t headerSize = sizeof(TXProtocolPacket);

    if (packet->dataSize <= headerSize + LZ4_MIN_DATA_SIZE) {
        ((TXProtocolPacket*)packet->data)->flags &= ~txFlagMasks::compressedPacketMask;
        return false;
    }

    char* sourcePayload = (char*)(packet->data + headerSize);
    int sourceSize = packet->dataSize - headerSize;

    int compressedSize = LZ4_compress_fast_extState(
        lz4Workspace, 
        sourcePayload, 
        (char*)compressedBuffer, 
        sourceSize, 
        sizeof(compressedBuffer), 
        LZ4_ACCEL
    );

    // Double check that the compression actually compressed the packet
    if (compressedSize > 0 && compressedSize < sourceSize) {
        memcpy(sourcePayload, compressedBuffer, compressedSize);
        size_t originalSize = packet->dataSize;
        packet->dataSize = headerSize + compressedSize;

        // Set the flag bit so the receiver knows to decompress
        ((TXProtocolPacket*)packet->data)->flags |= txFlagMasks::compressedPacketMask;
        
        ESP_LOGI(TAG, "Compressed: %d bytes → %d bytes (%.1f%% reduction)", 
            originalSize, 
            packet->dataSize,
            100.0f * (1.0f - ((float)packet->dataSize / (float)originalSize)));
        return true;
    } else {
        // Fail or Expansion: Ensure the flag is cleared so it's treated as RAW
        ((TXProtocolPacket*)packet->data)->flags &= ~txFlagMasks::compressedPacketMask;
        return false;
    }
}



bool decompressPacket(driverSendPacket* packet) {
    const uint8_t headerSize = sizeof(TXProtocolPacket);

    // Confirm if the compressed flag is set before attempting decompress
    if (((TXProtocolPacket*)packet->data)->flags & txFlagMasks::compressedPacketMask) {
        ESP_LOGW(TAG, "decompressedPacket called on uncompressed packet");
    }

    char* compressedPayload = (char*)(packet->data + headerSize);
    int compressedSize = packet->dataSize - headerSize;

    int decompressedSize = LZ4_decompress_safe(
        compressedPayload, 
        (char*)compressedBuffer, 
        compressedSize, 
        sizeof(compressedBuffer) 
    );

    if (decompressedSize < 0) {
        ESP_LOGE(TAG, "LZ4 decompression failed with code %d", decompressedSize);
        return false;
    }

    // Copy decompressed payload back over compressed payload (in-place)
    memcpy(compressedPayload, compressedBuffer, decompressedSize);
    packet->dataSize = headerSize + decompressedSize;

    // Clear flag bit after successful decompression
    ((TXProtocolPacket*)packet->data)->flags &= ~txFlagMasks::compressedPacketMask;
        return true;
}


