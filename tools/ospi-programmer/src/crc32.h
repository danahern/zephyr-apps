#ifndef CRC32_H
#define CRC32_H

#include <stdint.h>

/* Compute CRC32 (ISO 3309) over a buffer. */
uint32_t crc32(const void *data, uint32_t length);

#endif /* CRC32_H */
