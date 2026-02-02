# Zephyr Applications

Embedded applications built on the Zephyr RTOS.

## Development Guidelines

**IMPORTANT:** When working on code in this repository, follow the embedded development skill:

```
/embedded
```

This loads guidelines for memory management, error handling, code organization, and Zephyr patterns. Key points:
- Static allocation only (no malloc/free)
- Always check return codes
- Keep ISRs short, defer work to work queues
- Use Zephyr primitives (k_sem, k_mutex, k_msgq)

## Environment

**Always activate the virtual environment before running commands:**

```bash
source .venv/bin/activate
source ../zephyr/zephyr-env.sh
```

## Building

```bash
west build -b <board> apps/<app_name> -d apps/<app_name>/build
```

## Flashing

```bash
west flash -d apps/<app_name>/build
```

## ESP32 Prerequisites

```bash
pip install "esptool>=5.0.2"
west blobs fetch hal_espressif
```

## Structure

- `apps/` - Application projects
- `boards/` - Custom board definitions
- `drivers/` - Custom drivers
- `lib/` - Shared libraries

## Supported Boards

| Board | Identifier |
|-------|------------|
| nRF52840 DK | `nrf52840dk/nrf52840` |
| ESP32 DevKitC | `esp32_devkitc/esp32/procpu` |
| ESP32-S3-EYE | `esp32s3_eye/esp32s3/procpu` |

## Adding New Applications

1. Create `apps/my-app/src/main.c`
2. Add `CMakeLists.txt` and `prj.conf`
3. Test on hardware early
4. Update README.md
