#ifndef PCAP_H
#define PCAP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
    char magic[4]; // magic
    uint64_t start_time; // start time in us (esp_timer_get_time) when logging started
} __attribute__((packed)) pcap_header_s;
#define DEFAULT_PCAP_HEADER_S {.magic = { 'M', 'I', 'L', 'A' }, .start_time = 0}

typedef struct {
    uint32_t time_delta; // dt in us since previous record (saturates at UINT32_MAX)
    uint32_t id;
    struct {
        uint8_t flags: 4; // Only 1 = EXTD (0 = normal)
        uint8_t dlc: 4;
    } __attribute((packed)) dlc_flags;

    uint8_t data[8];
    uint8_t crc;
} __attribute__((packed)) pcap_record_s;

_Static_assert(sizeof(pcap_header_s) == 12, "PCAP Header Size should be 12 bytes but is not packed properly");
_Static_assert(sizeof(pcap_record_s) == 18, "PCAP Record Size should be 18 bytes but is not packed properly");

// === 1 Byte CRC ===
// We are using CRC-8-SAE J1850
void j1850_init_table(void); // Initialize the table for the CRC check
uint8_t j1850_compute(const uint8_t *data, size_t len); // Compute the CRC

#ifdef __cplusplus
}
#endif

#endif
