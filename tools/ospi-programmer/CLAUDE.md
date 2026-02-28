# OSPI RTT Flash Programmer (BROKEN)

Bare-metal M55_HP firmware for OSPI NOR flash programming via SEGGER RTT.

## Status: BROKEN

**M55_HP CPU cannot access the OSPI controller.** Access to 0x83002000 causes IMPRECISERR BusFault (CFSR=0x400) due to the EXPMST0 bus master bridge not forwarding M55_HP CPU accesses to AXI addresses in the 0x8xxx_xxxx range. The debug DAP can access these registers (bypasses EXPMST), but M55_HP CPU code cannot.

Until the EXPMST bridge configuration is resolved, use `jlink_flash` with FLM (~7 KB/s) for OSPI programming.

## Original Purpose

Was intended to replace the slow FLM-based JLink OSPI programming (~7 KB/s) with direct OSPI controller access. The ~500 KB/s speed estimate was never validated on hardware.

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
