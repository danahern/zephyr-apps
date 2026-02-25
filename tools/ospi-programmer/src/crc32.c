/*
 * CRC32 (ISO 3309 / ITU-T V.42) — bitwise implementation.
 * Polynomial: 0xEDB88320 (reversed representation of 0x04C11DB7).
 * Used for flash verify command.
 */

#include "crc32.h"

#define CRC32_POLY 0xEDB88320

uint32_t crc32(const void *data, uint32_t length)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFF;

    for (uint32_t i = 0; i < length; i++) {
        crc ^= p[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ CRC32_POLY;
            else
                crc = crc >> 1;
        }
    }

    return crc ^ 0xFFFFFFFF;
}
