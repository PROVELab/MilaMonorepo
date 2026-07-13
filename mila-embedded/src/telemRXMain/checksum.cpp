#include <stdint.h>

uint16_t in_cksum(const uint8_t* addr, int len) {
    uint32_t sum = 0;
    int i = 0;

    // Sum 16-bit words
    while (len > 1) {
        // Build 16-bit word from two bytes to avoid alignment issues.
        // Assumes Little Endian, which matches the Java implementation.
        uint16_t word = addr[i] | (uint16_t)(addr[i+1] << 8);
        sum += word;
        i += 2;
        len -= 2;
    }

    // Add odd byte if necessary
    if (len == 1) {
        sum += addr[i];
    }

    // Fold 32-bit sum to 16 bits and add carry
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    return (uint16_t) ~sum;
}