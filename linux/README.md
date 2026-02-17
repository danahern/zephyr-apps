# Linux for STM32MP1 Cortex-A7

Buildroot-based Linux image and userspace applications for the STM32MP1 A7 core.

## Buildroot SD Card Image

Builds a bootable SD card image using upstream Buildroot's `stm32mp157c_dk2_defconfig`. Includes TF-A, U-Boot, Linux kernel, and a minimal root filesystem.

### Build Image

```bash
# Build the Docker image (one-time)
docker build -t stm32mp1-sdk firmware/linux/docker/

# Using linux-build MCP:
start_container(name="stm32mp1-build", image="stm32mp1-sdk")
run_command(container, "git clone --depth 1 https://github.com/buildroot/buildroot.git /home/builder/buildroot")
run_command(container, "make stm32mp157c_dk2_defconfig", workdir="/home/builder/buildroot")
build(container, command="make -j$(nproc)", workdir="/home/builder/buildroot")
run_command(container, "cp /home/builder/buildroot/output/images/sdcard.img /artifacts/")
collect_artifacts(container, host_path="/tmp/stm32mp1-artifacts")
```

First build takes 30-60 minutes. Subsequent builds are incremental.

### Flash to SD Card

```bash
# Find your SD card device (DO NOT use your system disk)
# macOS: diskutil list    Linux: lsblk
sudo dd if=/tmp/stm32mp1-artifacts/sdcard.img of=/dev/sdX bs=4M status=progress
sync
```

### Image Artifacts

| File | Description |
|------|-------------|
| `sdcard.img` | Complete SD card image (GPT: TF-A + U-Boot + rootfs) |
| `tf-a-stm32mp157c-dk2.stm32` | TF-A first-stage bootloader |
| `fip.bin` | FIP image containing U-Boot |
| `zImage` | Linux kernel |
| `stm32mp157c-dk2.dtb` | Device tree blob |

### Boot

1. Insert SD card into the STM32MP157C-DK2
2. Connect USB-C for power and ST-Link serial console (115200 baud)
3. Set boot switches to SD card boot (SW1: BOOT0=0, BOOT2=1)
4. Login: `root` (no password)

## Apps

| App | Description |
|-----|-------------|
| `hello` | Print system info (kernel version, machine type) |
| `rpmsg_echo` | RPMsg echo client — send messages to M4 core via `/dev/rpmsg_charN` |

## Building

Apps are cross-compiled inside the Buildroot Docker container using the linux-build MCP.

### Prerequisites

1. Build the Docker image (see `docker/Dockerfile`)
2. Complete a Buildroot build (produces the cross-toolchain in `/buildroot/output/host/bin/`)

### MCP Workflow

```
# Start container with workspace mounted
linux-build.start_container(name="stm32mp1-build", image="stm32mp1-sdk", workspace_dir="/path/to/work")

# Build all apps (after Buildroot build has completed)
linux-build.build(container="stm32mp1-build", command="make -C /workspace/firmware/linux/apps all install", workdir="/workspace")

# Collect binaries
linux-build.collect_artifacts(container="stm32mp1-build", host_path="/tmp/stm32mp1-apps")

# Deploy to board
linux-build.deploy(file_path="/tmp/stm32mp1-apps/hello", board_ip="192.168.1.100")

# Run on board
linux-build.ssh_command(command="/home/root/hello", board_ip="192.168.1.100")
```

### Manual Cross-Compilation

If you have a cross-toolchain on the host:

```bash
CROSS_COMPILE=arm-linux-gnueabihf- make -C apps/hello
```

## RPMsg Echo

The `rpmsg_echo` app communicates with the M4 core via RPMsg. Prerequisites:

1. M4 firmware with an RPMsg echo endpoint (loaded via remoteproc or OpenOCD)
2. `rpmsg_char` kernel module loaded on A7
3. `/dev/rpmsg_ctrl0` device exists

```bash
# On the board
modprobe rpmsg_char
./rpmsg_echo -n 5 "Hello M4"
```

## Directory Structure

```
linux/
├── apps/
│   ├── Makefile          # Top-level: builds all apps
│   ├── hello/            # Simple hello world
│   │   ├── main.c
│   │   └── Makefile
│   └── rpmsg_echo/       # RPMsg IPC client
│       ├── main.c
│       └── Makefile
├── docker/               # Buildroot build environment (Ubuntu 22.04)
│   └── Dockerfile
└── README.md             # This file
```
