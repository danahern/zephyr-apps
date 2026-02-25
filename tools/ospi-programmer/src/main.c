/*
 * OSPI RTT Flash Programmer — main command loop
 *
 * Bare-metal M55_HP firmware. Receives commands over SEGGER RTT channel 0,
 * programs OSPI NOR flash (IS25WX256) via the DWC SSI controller.
 *
 * Memory: ITCM for code, DTCM for stack/BSS/RTT buffers.
 */

#include <stdint.h>
#include <stddef.h>

#include "SEGGER_RTT.h"
#include "protocol.h"
#include "ospi_flash.h"
#include "crc32.h"

/* RTT buffers in DTCM (BSS) */
static char rtt_up_buf[BUFFER_SIZE_UP];
static char rtt_down_buf[BUFFER_SIZE_DOWN];

/* Command receive buffer: header + max payload */
static uint8_t cmd_buf[CMD_HEADER_SIZE + MAX_WRITE_CHUNK];
static uint32_t cmd_buf_pos;

/* Response buffer: header + max response data */
static uint8_t resp_buf[RESP_HEADER_SIZE + MAX_WRITE_CHUNK];

static void send_response(uint8_t cmd_id, uint8_t status, uint16_t seq,
                          const void *data, uint32_t data_len)
{
    resp_header_t *resp = (resp_header_t *)resp_buf;
    resp->resp_id = cmd_id | RESP_FLAG;
    resp->status = status;
    resp->seq = seq;
    resp->length = data_len;

    uint32_t total = RESP_HEADER_SIZE + data_len;
    if (data && data_len > 0) {
        /* Copy data after header */
        const uint8_t *src = (const uint8_t *)data;
        for (uint32_t i = 0; i < data_len; i++)
            resp_buf[RESP_HEADER_SIZE + i] = src[i];
    }

    /* Write full response — block until sent */
    uint32_t sent = 0;
    while (sent < total) {
        unsigned n = SEGGER_RTT_Write(0, resp_buf + sent, total - sent);
        sent += n;
    }
}

static void send_ok(uint8_t cmd_id, uint16_t seq)
{
    send_response(cmd_id, STATUS_OK, seq, NULL, 0);
}

static void send_error(uint8_t cmd_id, uint16_t seq, uint8_t status)
{
    send_response(cmd_id, status, seq, NULL, 0);
}

/*
 * Convert address: if >= 0xC0000000 (XIP base), strip to flash-relative.
 * Otherwise treat as flash-relative already.
 */
static uint32_t to_flash_addr(uint32_t addr)
{
    if (addr >= 0xC0000000)
        return addr - 0xC0000000;
    return addr;
}

static void handle_ping(const cmd_header_t *cmd)
{
    /* Respond with firmware version in data */
    static const char version[] = "OSPI-RTT v1.0";
    send_response(cmd->cmd_id, STATUS_OK, cmd->seq,
                  version, sizeof(version) - 1);
}

static void handle_read_id(const cmd_header_t *cmd)
{
    uint8_t id = ospi_flash_read_id();
    send_response(cmd->cmd_id, STATUS_OK, cmd->seq, &id, 1);
}

static void handle_erase(const cmd_header_t *cmd)
{
    uint32_t flash_addr = to_flash_addr(cmd->addr);
    uint32_t remaining = cmd->length;
    uint32_t sector_size = 0x10000; /* 64KB */

    /* Align start down to sector boundary */
    uint32_t start = flash_addr & ~(sector_size - 1);
    uint32_t end = flash_addr + remaining;

    while (start < end) {
        if (ospi_flash_erase_sector(start) != 0) {
            send_error(cmd->cmd_id, cmd->seq, STATUS_TIMEOUT);
            return;
        }
        start += sector_size;
    }

    send_ok(cmd->cmd_id, cmd->seq);
}

static void handle_write(const cmd_header_t *cmd, const uint8_t *data)
{
    uint32_t flash_addr = to_flash_addr(cmd->addr);
    uint32_t remaining = cmd->length;
    uint32_t offset = 0;
    uint32_t page_size = 256;

    while (remaining > 0) {
        uint32_t chunk = remaining;
        if (chunk > page_size)
            chunk = page_size;

        /* Don't cross page boundary */
        uint32_t page_offset = (flash_addr + offset) & (page_size - 1);
        if (page_offset + chunk > page_size)
            chunk = page_size - page_offset;

        if (ospi_flash_program_page(flash_addr + offset,
                                    data + offset, chunk) != 0) {
            send_error(cmd->cmd_id, cmd->seq, STATUS_TIMEOUT);
            return;
        }

        offset += chunk;
        remaining -= chunk;
    }

    send_ok(cmd->cmd_id, cmd->seq);
}

static void handle_verify(const cmd_header_t *cmd)
{
    uint32_t flash_addr = to_flash_addr(cmd->addr);
    uint32_t length = cmd->length;

    /* Read flash data via XIP and compute CRC32 */
    volatile uint8_t *xip_ptr = (volatile uint8_t *)(0xC0000000 + flash_addr);

    /* Enter XIP temporarily for reading */
    ospi_flash_enter_xip();

    uint32_t crc = crc32((const void *)xip_ptr, length);

    /* Exit XIP back to command mode */
    ospi_flash_exit_xip();

    send_response(cmd->cmd_id, STATUS_OK, cmd->seq, &crc, 4);
}

static void handle_read(const cmd_header_t *cmd)
{
    uint32_t flash_addr = to_flash_addr(cmd->addr);
    uint32_t length = cmd->length;

    if (length > MAX_WRITE_CHUNK) {
        send_error(cmd->cmd_id, cmd->seq, STATUS_BAD_PARAM);
        return;
    }

    /* Read via XIP */
    volatile uint8_t *xip_ptr = (volatile uint8_t *)(0xC0000000 + flash_addr);

    ospi_flash_enter_xip();

    /* Copy to response buffer (after header) */
    uint8_t *data = resp_buf + RESP_HEADER_SIZE;
    for (uint32_t i = 0; i < length; i++)
        data[i] = xip_ptr[i];

    ospi_flash_exit_xip();

    send_response(cmd->cmd_id, STATUS_OK, cmd->seq, data, length);
}

static void handle_reset_flash(const cmd_header_t *cmd)
{
    ospi_flash_software_reset();
    send_ok(cmd->cmd_id, cmd->seq);
}

static void process_command(void)
{
    cmd_header_t *cmd = (cmd_header_t *)cmd_buf;
    uint8_t *data = cmd_buf + CMD_HEADER_SIZE;

    switch (cmd->cmd_id) {
    case CMD_PING:
        handle_ping(cmd);
        break;
    case CMD_READ_ID:
        handle_read_id(cmd);
        break;
    case CMD_ERASE:
        handle_erase(cmd);
        break;
    case CMD_WRITE:
        handle_write(cmd, data);
        break;
    case CMD_VERIFY:
        handle_verify(cmd);
        break;
    case CMD_READ:
        handle_read(cmd);
        break;
    case CMD_RESET_FLASH:
        handle_reset_flash(cmd);
        break;
    default:
        send_error(cmd->cmd_id, cmd->seq, STATUS_BAD_PARAM);
        break;
    }
}

int main(void)
{
    /* Init RTT with custom buffers */
    SEGGER_RTT_Init();
    SEGGER_RTT_ConfigUpBuffer(0, "Terminal", rtt_up_buf,
                               sizeof(rtt_up_buf),
                               SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL);
    SEGGER_RTT_ConfigDownBuffer(0, "Terminal", rtt_down_buf,
                                 sizeof(rtt_down_buf),
                                 SEGGER_RTT_MODE_NO_BLOCK_SKIP);

    /* Initialize OSPI flash controller */
    ospi_flash_init();

    /* Main command loop */
    cmd_buf_pos = 0;

    for (;;) {
        /* Read available bytes from RTT down buffer */
        unsigned avail = SEGGER_RTT_HasData(0);
        if (avail == 0)
            continue;

        unsigned n = SEGGER_RTT_Read(0, cmd_buf + cmd_buf_pos,
                                     sizeof(cmd_buf) - cmd_buf_pos);
        if (n == 0)
            continue;

        cmd_buf_pos += n;

        /* Need at least the header to know the command */
        if (cmd_buf_pos < CMD_HEADER_SIZE)
            continue;

        /* For CMD_WRITE, need header + data payload */
        cmd_header_t *cmd = (cmd_header_t *)cmd_buf;
        uint32_t expected;

        if (cmd->cmd_id == CMD_WRITE) {
            if (cmd->length > MAX_WRITE_CHUNK) {
                send_error(cmd->cmd_id, cmd->seq, STATUS_BAD_PARAM);
                cmd_buf_pos = 0;
                continue;
            }
            expected = CMD_HEADER_SIZE + cmd->length;
        } else {
            expected = CMD_HEADER_SIZE;
        }

        if (cmd_buf_pos < expected)
            continue;

        /* Full command received — process it */
        process_command();
        cmd_buf_pos = 0;
    }

    return 0;
}
