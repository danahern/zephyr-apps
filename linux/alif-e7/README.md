# Linux for Alif Ensemble E7 DevKit

Yocto-based Linux image and cross-compiled userspace apps for the Alif E7 Cortex-A32 cores.

## Architecture

The E7 has a heterogeneous architecture:
- **2x Cortex-A32** — Linux (this directory)
- **2x Cortex-M55** — Zephyr RTOS
- **Ethos-U55** — NPU for ML inference

Boot chain: Secure Enclave → TF-A → xipImage (no U-Boot). Kernel runs XIP from MRAM.

## Yocto Image Build

### Prerequisites

1. Docker Desktop with sufficient resources (8GB+ RAM, 50GB+ disk)
2. Existing Yocto Docker image: `docker build -t yocto-builder firmware/linux/yocto/`

### Build

The E7 build uses the same Yocto Docker container as STM32MP1, with a separate build directory.

```bash
# Start container with yocto-data volume + artifacts bind mount
docker run -dit --name yocto-build \
  -v yocto-data:/home/builder/yocto \
  -v $(pwd)/yocto-build:/home/builder/artifacts \
  yocto-builder

# Copy BSP layers into Docker volume (first time only)
docker exec yocto-build bash -c "
  cd /home/builder/yocto &&
  git clone -b scarthgap https://github.com/alifsemi/meta-alif.git &&
  git clone -b scarthgap https://github.com/alifsemi/meta-alif-ensemble.git
"

# Copy build config
docker cp yocto-build/build-alif-e7/conf/. yocto-build:/home/builder/yocto/build-alif-e7/conf/

# Build
docker exec yocto-build bash -c "
  cd /home/builder/yocto &&
  source poky/oe-init-build-env build-alif-e7 &&
  bitbake core-image-minimal
"
```

### Machine Config

Uses `MACHINE = "devkit-e8"` (the `devkit-e7.conf` was superseded in the scarthgap branch). The E8 config still targets E7 hardware (`TF-A_PLATFORM = "devkit_e7"`).

### Output Artifacts

| File | Description |
|------|-------------|
| `xipImage` | XIP Linux kernel (runs from MRAM) |
| `bl32.bin` | TF-A BL32 (Secure Payload) |
| `devkit-e8.dtb` | Device tree blob |
| `cramfs-xip` | Read-only root filesystem |

### Flashing

Flashing requires Alif's SETOOLS/ATOC to package images for the Secure Enclave boot chain. This differs from STM32MP1's simple `dd` to SD card.

```
# TBD — requires SETOOLS validation with actual hardware
```

## App Cross-Compilation

### Docker Image

A lightweight Docker image with the system ARM cross-compiler (no Buildroot needed):

```bash
docker build -t alif-e7-sdk -f firmware/linux/docker/Dockerfile.alif-e7 .
```

### MCP Workflow

```
linux-build.start_container(name="alif-e7-build", image="alif-e7-sdk", workspace_dir="/path/to/work")
linux-build.build(container="alif-e7-build", command="make -C /workspace/firmware/linux/apps BOARD=alif-e7 all install")
linux-build.collect_artifacts(container="alif-e7-build", host_path="/tmp/alif-e7-apps")
```

### Manual Build

```bash
BOARD=alif-e7 make -C firmware/linux/apps
```

## Comparison with STM32MP1

| | STM32MP1 | Alif E7 |
|---|---|---|
| CPU | Cortex-A7 (1x) | Cortex-A32 (2x SMP) |
| Boot media | SD card (WIC) | MRAM/OSPI (xipImage) |
| Bootloader | TF-A + U-Boot | TF-A + Secure Enclave |
| Flash tool | dd to SD | SETOOLS/ATOC |
| Cross-compiler | Buildroot toolchain | System `arm-linux-gnueabihf-gcc` |
| CPU flags | `-mcpu=cortex-a7 -mfpu=neon-vfpv4` | `-mcpu=cortex-a32 -mfpu=neon` |
| Yocto machine | `stm32mp1` | `devkit-e8` |
