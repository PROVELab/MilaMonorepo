#ifndef PCAP_H
#define PCAP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char magic[4] = { 'M', 'I', 'L', 'A' };
    const uint64_t start_time = 0; // start time (when the logging started)
} __attribute__((packed)) pcap_header_s;

typedef struct {
    uint32_t id;
    struct {
        uint8_t flags: 4; // Only 1 = EXTD (0 = normal)
        uint8_t dlc: 4;
    } __attribute((packed)) dlc_flags;

    uint8_t data[8];
    uint8_t crc;
} __attribute__((packed)) pcap_record_s;

static_assert(sizeof(pcap_header_s) == 12, "PCAP Header Size should be 12 bytes but is not packed properly");
static_assert(sizeof(pcap_record_s) == 14, "PCAP Record Size should be 14 bytes but is not packed properly");

// === 1 Byte CRC ===
// We are using CRC-8-SAE J1850
void j1850_init_table(void); // Initialize the table for the CRC check
uint8_t j1850_compute(const uint8_t *data, size_t len); // Compute the CRC

#ifdef __cplusplus
}
#endif

#endif

