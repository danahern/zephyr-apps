# Linux Userspace Apps

Cross-compiled Linux userspace applications. Supports multiple boards via `BOARD=` variable.

## Supported Boards

| Board | CPU | Docker Image | BOARD= | Toolchain |
|-------|-----|-------------|--------|-----------|
| STM32MP1 DK | Cortex-A7 | `stm32mp1-sdk` | _(default)_ | Buildroot `arm-none-linux-gnueabihf-gcc` |
| Alif E7 DevKit | Cortex-A32 | `alif-e7-sdk` | `alif-e7` | System `arm-linux-gnueabihf-gcc` |

## Apps

| App | Description |
|-----|-------------|
| `hello` | Print system info (kernel version, machine type) |
| `rpmsg_echo` | RPMsg echo client — send messages to M4 core via `/dev/rpmsg_charN` |

## Building

### Docker Images

```bash
# STM32MP1 (Buildroot-based, requires prior Buildroot build for toolchain)
docker build -t stm32mp1-sdk firmware/linux/docker/

# Alif E7 (system cross-compiler, no prior build needed)
docker build -t alif-e7-sdk -f firmware/linux/docker/Dockerfile.alif-e7 firmware/linux/docker/
```

### MCP Workflow

```
# STM32MP1
linux-build.start_container(name="stm32mp1-build", image="stm32mp1-sdk", workspace_dir="/path/to/work")
linux-build.build(container, command="make -C /workspace/firmware/linux/apps all install")

# Alif E7
linux-build.start_container(name="alif-e7-build", image="alif-e7-sdk", workspace_dir="/path/to/work")
linux-build.build(container, command="make -C /workspace/firmware/linux/apps BOARD=alif-e7 all install")

# Both: collect and deploy
linux-build.collect_artifacts(container, host_path="/tmp/apps")
linux-build.deploy(file_path="/tmp/apps/hello", board_ip="192.168.x.x")
linux-build.ssh_command(command="/home/root/hello", board_ip="192.168.x.x")
```

### Manual Cross-Compilation

```bash
# STM32MP1 (if you have Buildroot toolchain on host)
CROSS_COMPILE=arm-linux-gnueabihf- make -C apps/hello

# Alif E7
make -C apps/hello BOARD=alif-e7
```

## Yocto Images

Each board has a separate Yocto build configuration in `yocto-build/`:

| Board | Build Dir | Machine | Layers |
|-------|-----------|---------|--------|
| STM32MP1 | `build-stm32mp/` | `stm32mp1` | poky + meta-oe + meta-python + meta-st-stm32mp |
| Alif E7 | `build-alif-e7/` | `devkit-e8` | poky + meta-oe + meta-python + meta-filesystems + meta-alif + meta-alif-ensemble |

Both share the same Docker container (`yocto-builder`) and Docker volume (`yocto-data`), including downloads and sstate-cache.

See `alif-e7/README.md` for E7-specific build and flash instructions.

## Directory Structure

```
linux/
├── apps/
│   ├── Makefile              # Top-level: builds all apps (pass BOARD=)
│   ├── hello/                # Simple hello world
│   │   ├── main.c
│   │   └── Makefile
│   └── rpmsg_echo/           # RPMsg IPC client
│       ├── main.c
│       └── Makefile
├── alif-e7/
│   └── README.md             # Alif E7 build + flash docs
├── docker/
│   ├── Dockerfile            # Buildroot build environment (STM32MP1)
│   └── Dockerfile.alif-e7    # System cross-compiler (Alif E7)
├── yocto/
│   └── Dockerfile            # Yocto build environment (shared)
└── README.md                 # This file
```
