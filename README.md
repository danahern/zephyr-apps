# zephyr-apps

Embedded applications built on the Zephyr RTOS.

## Prerequisites

### pyenv (Python Version Management)

This project uses pyenv to manage Python versions. The required version is specified in `.python-version`.

#### macOS

```bash
# Install pyenv via Homebrew
brew install pyenv

# Add to shell (bash)
echo 'export PYENV_ROOT="$HOME/.pyenv"' >> ~/.bashrc
echo 'command -v pyenv >/dev/null || export PATH="$PYENV_ROOT/bin:$PATH"' >> ~/.bashrc
echo 'eval "$(pyenv init -)"' >> ~/.bashrc

# Add to shell (zsh)
echo 'export PYENV_ROOT="$HOME/.pyenv"' >> ~/.zshrc
echo 'command -v pyenv >/dev/null || export PATH="$PYENV_ROOT/bin:$PATH"' >> ~/.zshrc
echo 'eval "$(pyenv init -)"' >> ~/.zshrc

# Restart shell
exec "$SHELL"
```

#### Linux

```bash
# Install build dependencies (Debian/Ubuntu)
sudo apt update && sudo apt install -y \
    build-essential libssl-dev zlib1g-dev libbz2-dev \
    libreadline-dev libsqlite3-dev curl libncursesw5-dev \
    xz-utils tk-dev libxml2-dev libxmlsec1-dev libffi-dev liblzma-dev

# Install build dependencies (Fedora/RHEL)
sudo dnf install -y \
    gcc zlib-devel bzip2 bzip2-devel readline-devel sqlite sqlite-devel \
    openssl-devel tk-devel libffi-devel xz-devel

# Install pyenv
curl https://pyenv.run | bash

# Add to shell (bash)
echo 'export PYENV_ROOT="$HOME/.pyenv"' >> ~/.bashrc
echo 'command -v pyenv >/dev/null || export PATH="$PYENV_ROOT/bin:$PATH"' >> ~/.bashrc
echo 'eval "$(pyenv init -)"' >> ~/.bashrc

# Add to shell (zsh)
echo 'export PYENV_ROOT="$HOME/.pyenv"' >> ~/.zshrc
echo 'command -v pyenv >/dev/null || export PATH="$PYENV_ROOT/bin:$PATH"' >> ~/.zshrc
echo 'eval "$(pyenv init -)"' >> ~/.zshrc

# Restart shell
exec "$SHELL"
```

#### Install the Required Python Version

```bash
# Install the version specified in .python-version
pyenv install

# Verify
python --version  # Should show 3.11.9
```

### CMake 3.20+

#### macOS

```bash
brew install cmake
```

#### Linux

```bash
# Debian/Ubuntu
sudo apt install -y cmake

# If version is too old, install from Kitware repo:
sudo apt install -y gpg wget
wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc | sudo gpg --dearmor -o /usr/share/keyrings/kitware-archive-keyring.gpg
echo "deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ $(lsb_release -cs) main" | sudo tee /etc/apt/sources.list.d/kitware.list
sudo apt update && sudo apt install -y cmake

# Fedora/RHEL
sudo dnf install -y cmake
```

### Zephyr SDK

#### macOS

```bash
# Install dependencies
brew install ninja gperf ccache qemu dtc wget

# Download and install Zephyr SDK
cd ~
wget https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.16.8/zephyr-sdk-0.16.8_macos-x86_64.tar.xz
tar xf zephyr-sdk-0.16.8_macos-x86_64.tar.xz
cd zephyr-sdk-0.16.8
./setup.sh
```

#### Linux

```bash
# Install dependencies (Debian/Ubuntu)
sudo apt install -y --no-install-recommends \
    git ninja-build gperf ccache dfu-util \
    device-tree-compiler wget xz-utils file \
    make gcc gcc-multilib g++-multilib libsdl2-dev

# Install dependencies (Fedora/RHEL)
sudo dnf install -y git ninja-build gperf ccache dfu-util \
    dtc wget xz file make gcc gcc-c++ SDL2-devel

# Download and install Zephyr SDK
cd ~
wget https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.16.8/zephyr-sdk-0.16.8_linux-x86_64.tar.xz
tar xf zephyr-sdk-0.16.8_linux-x86_64.tar.xz
cd zephyr-sdk-0.16.8
./setup.sh
```

## Setup

1. Clone this repository:
   ```bash
   git clone https://github.com/<your-username>/zephyr-apps.git
   cd zephyr-apps
   ```

2. Set up Python environment with pyenv:
   ```bash
   # Install the required Python version (reads .python-version)
   pyenv install

   # Verify the correct version is active
   python --version  # Should show 3.11.9

   # Create a virtual environment
   python -m venv .venv
   source .venv/bin/activate

   # Install base dependencies
   pip install -r requirements.txt
   ```

3. Initialize the west workspace:
   ```bash
   west init -l .
   west update
   ```

4. Install Zephyr Python dependencies:
   ```bash
   pip install -r zephyr/scripts/requirements.txt
   ```

5. Set up the Zephyr environment:
   ```bash
   source zephyr/zephyr-env.sh
   ```

## Activating the Environment

Each time you start a new terminal session, activate the environment before working:

```bash
cd /path/to/zephyr-apps

# Activate the Python virtual environment
source .venv/bin/activate

# Set up Zephyr environment (if west workspace is initialized)
source zephyr/zephyr-env.sh
```

You can add this to a helper script or your shell profile for convenience.

## Structure

```
zephyr-apps/
├── .python-version  # pyenv Python version
├── .venv/           # Virtual environment (created during setup)
├── requirements.txt # Python dependencies
├── west.yml         # West manifest
├── apps/            # Application projects
│   ├── ble_data_transfer/  # BLE NUS echo app (nRF52840)
│   └── ble_wifi_bridge/    # BLE-to-WiFi bridge (ESP32-S3-EYE)
├── tools/           # Testing and utility scripts
│   └── ble_throughput.py   # BLE throughput tester
├── boards/          # Custom board definitions
├── drivers/         # Custom drivers
└── lib/             # Shared libraries
```

## Applications

### ble_data_transfer

A BLE peripheral using Nordic UART Service (NUS) for bidirectional data transfer. Echoes received data back to the connected central device.

**Target Board:** nRF52840DK (`nrf52840dk/nrf52840`)

**Features:**
- BLE peripheral with NUS (Nordic UART Service)
- Echoes received data with "Echo: " prefix
- Sends periodic status updates every 10 seconds
- Automatic re-advertising on disconnect

**Build:**
```bash
source .venv/bin/activate
west build -b nrf52840dk/nrf52840 apps/ble_data_transfer -d apps/ble_data_transfer/build
```

**Flash:**
```bash
west flash -d apps/ble_data_transfer/build
```

### ble_wifi_bridge

A BLE-to-WiFi bridge that receives data over BLE (NUS) and forwards it to a TCP server over WiFi.

**Target Board:** ESP32-S3-EYE (`esp32s3_eye/esp32s3/procpu`)

**Features:**
- BLE peripheral with NUS for receiving data
- WiFi client with configurable SSID/password
- TCP socket client for forwarding data
- Bidirectional bridging (BLE ↔ TCP)
- Connection management for both BLE and WiFi

**Prerequisites (ESP32):**

Before building for ESP32, install esptool and fetch the required binary blobs:
```bash
source .venv/bin/activate
pip install "esptool>=5.0.2"
west blobs fetch hal_espressif
```

**Build:**
```bash
source .venv/bin/activate
west build -b esp32s3_eye/esp32s3/procpu apps/ble_wifi_bridge -d apps/ble_wifi_bridge/build
```

**Flash:**
```bash
west flash -d apps/ble_wifi_bridge/build
```

**Configuration:**

Edit `apps/ble_wifi_bridge/prj.conf` to set WiFi and TCP server settings:
```
CONFIG_BRIDGE_WIFI_SSID="YourNetworkName"
CONFIG_BRIDGE_WIFI_PSK="YourPassword"
CONFIG_BRIDGE_TCP_SERVER_ADDR="192.168.1.100"
CONFIG_BRIDGE_TCP_SERVER_PORT=4242
```

## Tools

### ble_throughput.py

A Python tool for testing BLE throughput with devices using Nordic UART Service (NUS).

**Features:**
- Scan for BLE devices
- TX throughput test (sustained writes)
- Echo latency test (round-trip timing)
- Burst transfer test (packet bursts)
- Detailed statistics reporting

**Usage:**

```bash
# Activate the virtual environment
source .venv/bin/activate

# Scan for BLE devices
python tools/ble_throughput.py --scan

# Connect by device name and run all tests
python tools/ble_throughput.py --name "BLE Data Transfer" --test all

# Connect by address and run specific test
python tools/ble_throughput.py --addr "AA:BB:CC:DD:EE:FF" --test echo

# Run TX test with custom parameters
python tools/ble_throughput.py --name "BLE Data Transfer" --test tx --duration 30 --size 200

# Run burst test
python tools/ble_throughput.py --name "BLE Data Transfer" --test burst --packets 500 --size 100
```

**Options:**
| Option | Description |
|--------|-------------|
| `--scan` | Scan for BLE devices only |
| `--name NAME` | Device name to connect to |
| `--addr ADDR` | Device MAC address to connect to |
| `--test {tx,echo,burst,all}` | Test type to run (default: all) |
| `--duration SECS` | Test duration in seconds (default: 10) |
| `--size BYTES` | Packet size in bytes (default: 20, max ~240) |
| `--packets NUM` | Number of packets for burst test (default: 100) |

**Example Output:**
```
==================================================
THROUGHPUT TEST RESULTS
==================================================
Duration:        10.05 sec
Packets sent:    892
Packets recv:    891
Bytes sent:      17,840
Bytes received:  22,275
TX throughput:   1775.12 B/s (14.20 kbit/s)
RX throughput:   2216.42 B/s (17.73 kbit/s)
Avg latency:     11.24 ms
Min latency:     8.12 ms
Max latency:     45.67 ms
==================================================
```

## Building Applications

```bash
# Activate the virtual environment
source .venv/bin/activate

# Build for a specific board
west build -b <board_name> apps/<app_name> -d apps/<app_name>/build

# Clean build (remove previous build artifacts)
west build -b <board_name> apps/<app_name> -d apps/<app_name>/build --pristine

# Examples
west build -b nrf52840dk/nrf52840 apps/ble_data_transfer -d apps/ble_data_transfer/build
west build -b esp32s3_eye/esp32s3/procpu apps/ble_wifi_bridge -d apps/ble_wifi_bridge/build
```

## Flashing

```bash
# Flash the most recently built app
west flash -d apps/<app_name>/build

# Examples
west flash -d apps/ble_data_transfer/build
west flash -d apps/ble_wifi_bridge/build
```

## Debugging

```bash
# Start debugger
west debug -d apps/<app_name>/build

# For ESP32, use OpenOCD or JTAG debugger
west debug -d apps/ble_wifi_bridge/build --runner openocd
```

## Monitoring Serial Output

```bash
# nRF52840 (via Segger RTT or UART)
# Use a serial terminal at 115200 baud

# ESP32-S3-EYE (via USB serial)
# macOS
screen /dev/tty.usbserial-* 115200

# Linux
screen /dev/ttyUSB0 115200

# Or use west
west espressif monitor -d apps/ble_wifi_bridge/build
```

## Creating a New Application

1. Create a directory under `apps/`:
   ```bash
   mkdir -p apps/my-app/src
   ```

2. Add `apps/my-app/CMakeLists.txt`:
   ```cmake
   cmake_minimum_required(VERSION 3.20.0)
   find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
   project(my-app)
   target_sources(app PRIVATE src/main.c)
   ```

3. Add `apps/my-app/prj.conf`:
   ```
   # Application configuration
   CONFIG_LOG=y
   ```

4. Add your source code in `apps/my-app/src/main.c`

5. Build:
   ```bash
   west build -b <board> apps/my-app -d apps/my-app/build
   ```

## Supported Boards

| Board | Identifier | Notes |
|-------|------------|-------|
| nRF52840 DK | `nrf52840dk/nrf52840` | Nordic dev kit, BLE 5.0 |
| ESP32-S3-EYE | `esp32s3_eye/esp32s3/procpu` | WiFi + BLE + Camera, requires blobs |
| nRF52 DK | `nrf52dk/nrf52832` | Nordic dev kit, BLE 5.0 |
| ESP32 DevKitC | `esp32_devkitc/esp32/procpu` | WiFi + BLE, requires blobs |

## Troubleshooting

### ESP32: "esptool not found"
```bash
pip install "esptool>=5.0.2"
```

### ESP32: "Blob missing" error
```bash
west blobs fetch hal_espressif
```

### Build fails with stale cache
```bash
rm -rf apps/<app_name>/build
west build -b <board> apps/<app_name> -d apps/<app_name>/build
```

### BLE throughput tool can't find device
- Ensure the device is powered and advertising
- Check that no other app (phone, etc.) is connected to it
- Try scanning first: `python tools/ble_throughput.py --scan`
- On macOS, grant Bluetooth permissions to Terminal
