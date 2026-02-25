/*
 * OSPI RTT Flash Programmer — Command/Response Protocol
 *
 * Binary, little-endian, on RTT channel 0.
 * Host sends one command, waits for response before next.
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

/* Command IDs (host -> target) */
#define CMD_PING        0x01
#define CMD_READ_ID     0x02
#define CMD_ERASE       0x03
#define CMD_WRITE       0x04
#define CMD_VERIFY      0x05
#define CMD_READ        0x06
#define CMD_RESET_FLASH 0x08

/* Response ID = cmd_id | 0x80 */
#define RESP_FLAG       0x80

/* Status codes */
#define STATUS_OK           0
#define STATUS_TIMEOUT      1
#define STATUS_VERIFY_FAIL  2
#define STATUS_BAD_PARAM    3
#define STATUS_FLASH_ERR    4

/* Max data per CMD_WRITE (4096 = 16 flash pages of 256B) */
#define MAX_WRITE_CHUNK     4096

/*
 * Command header (host -> target), 12 bytes.
 * For CMD_WRITE, followed by `length` bytes of data.
 */
typedef struct __attribute__((packed)) {
    uint8_t  cmd_id;
    uint8_t  flags;
    uint16_t seq;
    uint32_t addr;
    uint32_t length;
} cmd_header_t;

/*
 * Response header (target -> host), 8 bytes.
 * May be followed by `length` bytes of data (VERIFY: 4B CRC32, READ: raw).
 */
typedef struct __attribute__((packed)) {
    uint8_t  resp_id;
    uint8_t  status;
    uint16_t seq;
    uint32_t length;
} resp_header_t;

#define CMD_HEADER_SIZE  sizeof(cmd_header_t)   /* 12 */
#define RESP_HEADER_SIZE sizeof(resp_header_t)   /* 8 */

#endif /* PROTOCOL_H */
