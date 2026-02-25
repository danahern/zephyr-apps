# OSPI RTT Flash Programmer

Bare-metal M55_HP firmware for high-speed OSPI NOR flash programming via SEGGER RTT.

## Purpose

Replaces the slow FLM-based JLink OSPI programming (~7 KB/s, ~25 min for 9MB) with direct OSPI flash control over RTT. Expected throughput: ~500 KB/s sustained.

## Architecture

The firmware runs on the M55_HP core, receives binary commands over RTT channel 0, and directly controls the DWC SSI OSPI controller to erase/program the IS25WX256 flash.

## Build

```bash
make                    # Build with Zephyr SDK
make clean              # Clean build artifacts
```

Requires Zephyr SDK (arm-zephyr-eabi-gcc). Output: `build/ospi_programmer.bin` (~5KB).

## Memory Layout

| Region | Address | Size | Contents |
|--------|---------|------|----------|
| ITCM | 0x00000000 (local) / 0x50000000 (global) | 256KB | Code, rodata |
| DTCM | 0x20000000 | 256KB | Stack (top), BSS, RTT buffers |

ATOC `loadAddress: "0x50000000"` maps to local ITCM 0x00000000.

## RTT Protocol

Binary, little-endian, channel 0. Host sends command, waits for response.

**Commands**: PING(0x01), READ_ID(0x02), ERASE(0x03), WRITE(0x04), VERIFY(0x05), READ(0x06), RESET_FLASH(0x08)

**Key rules**:
- DFS=16 for ALL DDR Octal operations
- Data packed as 16-bit LE frames
- Software reset (0x66 + 0x99) after programming before XIP re-enter

## Key Files

- `src/main.c` — RTT init, command loop
- `src/ospi_drv.c/.h` — DWC SSI driver (ported from TF-A)
- `src/ospi_flash.c/.h` — Flash operations (init, erase, program, XIP)
- `src/protocol.h` — Command/response protocol definitions
- `src/crc32.c/.h` — CRC32 for verify
- `lib/SEGGER_RTT.*` — SEGGER RTT (copied from Zephyr modules)
