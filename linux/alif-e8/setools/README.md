# E8 SETOOLS Flashing

Flash Linux images to E8 MRAM via Alif's Secure Enclave UART.

## Prerequisites

1. **SETOOLS**: Same installation as E7 (`tools/setools/`). The toolkit supports both E7 and E8.

2. **Device config**: Run `tools/setools/tools-config` to select device **E8 (AE822FA0E5597LS0)**.
   Note: E8 device config (`app-device-config.json`) is board-specific — do NOT reuse E7's config.

3. **Hardware**: Connect micro-USB to **PRG_USB** port (SE-UART + console UART).

4. **Docker**: APSS build container with completed E8 Yocto build (scarthgap).

## Quick Start

```bash
# Use alif-flash MCP tool with device parameter:
alif-flash.gen_toc(config="firmware/linux/alif-e8/setools/linux-boot-e8.json")
alif-flash.maintenance(device="alif-e8")
alif-flash.flash(config="firmware/linux/alif-e8/setools/linux-boot-e8.json", device="alif-e8")
```

## Memory Map (devkit-e8.conf, scarthgap branch)

| Image | MRAM Address | Description |
|-------|-------------|-------------|
| bl32.bin | 0x80002000 | TF-A BL32 (same platform as E7: `devkit_e7`) |
| devkit-e8.dtb | 0x80010000 | Device tree blob (E8-specific) |
| xipImage | 0x80020000 | XIP Linux kernel (v6.12) |
| alif-tiny-image-devkit-e8.cramfs-xip | 0x80380000 | Read-only root filesystem |

## Key Differences from E7

- **Rootfs address**: 0x80380000 (E7: 0x80300000)
- **DTB**: `devkit-e8.dtb` (E7: `appkit-e7.dtb`)
- **Kernel**: v6.12 from scarthgap (E7: v5.4 from zeus)
- **Yocto syntax**: `:append` (E7: `_append`)
- **MACHINE**: `devkit-e8` (E7: `appkit-e7`)
- **TF-A platform**: Same (`devkit_e7`) — shared across E7/E8

## Build System

- Repo: `alifsemi/alif_linux-apss-build-setup` (scarthgap branch)
- Machine: `MACHINE="devkit-e8"`
- Kernel: `linux-alif_6.12.bb` recipe (v6.12 kernel)
- Build: `bitbake alif-tiny-image`

## Troubleshooting

See `firmware/linux/alif-e7/setools/README.md` for ISP protocol details — the SE-UART protocol is identical for E7 and E8.
