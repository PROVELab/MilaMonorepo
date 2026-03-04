#include "pcap.h"

static uint8_t crc8_sae_j1850_table[256];

void j1850_init_table(void)
{
    for (int i = 0; i < 256; i++) {
        uint8_t crc = i;
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x1D;
            else
                crc <<= 1;
        }
        crc8_sae_j1850_table[i] = crc;
    }
}

uint8_t j1850_compute(const uint8_t *data, size_t len)
{
    uint8_t crc = 0xFF;

    for (size_t i = 0; i < len; i++)
        crc = crc8_sae_j1850_table[crc ^ data[i]];

    return crc ^ 0xFF;
}



