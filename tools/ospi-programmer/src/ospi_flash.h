/*
 * OSPI flash API — high-level operations for the IS25WX256.
 *
 * Init, probe, erase, program, XIP enter/exit, software reset.
 * Ported from TF-A norflash_ospi_setup.c.
 */

#ifndef OSPI_FLASH_H
#define OSPI_FLASH_H

#include <stdint.h>

/* Initialize OSPI controller and probe flash (SPI single → DDR Octal). */
void ospi_flash_init(void);

/* Read JEDEC ID. Returns manufacturer ID byte (0x9D for ISSI IS25WX256). */
uint8_t ospi_flash_read_id(void);

/* Erase a 64KB sector. addr is flash-relative (0-based). Returns 0 on success. */
int ospi_flash_erase_sector(uint32_t addr);

/* Program up to 256 bytes (one page). addr is flash-relative. Returns 0 on success. */
int ospi_flash_program_page(uint32_t addr, const uint8_t *data, uint32_t len);

/* Enter XIP mode (for CRC verify reads). */
void ospi_flash_enter_xip(void);

/* Exit XIP and re-init for command mode. */
void ospi_flash_exit_xip(void);

/* Software reset flash to SPI single mode. */
void ospi_flash_software_reset(void);

#endif /* OSPI_FLASH_H */
