# WiFi Provisioning App

Thin application that uses the `wifi_prov` library to provide WiFi provisioning over BLE, with a TCP throughput server for performance testing.

## Supported Boards

| Board | Status | Notes |
|-------|--------|-------|
| nrf7002dk/nrf5340/cpuapp | Builds | nRF5340 + nRF7002 companion WiFi IC |
| esp32_devkitc/esp32/procpu | Needs esptool | ESP32 built-in WiFi |

## Build

```bash
# nRF7002-DK
zephyr-build.build(app="wifi_provision", board="nrf7002dk/nrf5340/cpuapp", pristine=true)

# ESP32 (requires: pip install "esptool>=5.0.2" && west blobs fetch hal_espressif)
zephyr-build.build(app="wifi_provision", board="esp32_devkitc/esp32/procpu", pristine=true)
```

## Flash & Run

### nRF7002-DK
```bash
# Connect J-Link, then:
embedded-probe.connect(probe_selector="auto", target_chip="nRF5340_xxAA")
embedded-probe.validate_boot(session_id, file_path="apps/wifi_provision/build/zephyr/zephyr.hex", success_pattern="WiFi Provisioning App")
```

### ESP32
```bash
esp-idf-build.flash(project="wifi_provision", port="/dev/ttyUSB0")
```

## Usage

1. Flash firmware to device
2. Device starts BLE advertising as "WiFi Prov"
3. Use the Python provisioning tool:

```bash
# Discover devices
python3 test-tools/wifi_provision_tool.py discover

# Scan for WiFi APs
python3 test-tools/wifi_provision_tool.py scan-aps <BLE_ADDRESS>

# Provision WiFi credentials
python3 test-tools/wifi_provision_tool.py provision <BLE_ADDRESS> "MySSID" "MyPassword"

# Check status
python3 test-tools/wifi_provision_tool.py status <BLE_ADDRESS>

# Factory reset
python3 test-tools/wifi_provision_tool.py factory-reset <BLE_ADDRESS>
```

4. Once connected to WiFi, run throughput tests:

```bash
# Upload test (10 seconds)
python3 test-tools/throughput_test.py <DEVICE_IP> upload

# Download test (30 seconds)
python3 test-tools/throughput_test.py <DEVICE_IP> download -d 30

# Echo (round-trip) test
python3 test-tools/throughput_test.py <DEVICE_IP> echo
```

## Throughput Server

The app includes a TCP throughput server on port 4242 with three modes:
- **Echo (0x01):** Client sends data, server echoes back
- **Sink (0x02):** Client sends data, server discards
- **Source (0x03):** Server sends data continuously

The server logs per-second throughput stats via RTT/UART.

## Configuration

Key Kconfig options (set in `prj.conf`):

| Option | Default | Description |
|--------|---------|-------------|
| `CONFIG_WIFI_PROV_AUTO_CONNECT` | y | Auto-connect from stored creds on boot |
| `CONFIG_BT_DEVICE_NAME` | "WiFi Prov" | BLE advertising name |

Board-specific configs are in `boards/`:
- `nrf7002dk_nrf5340_cpuapp.conf` — nRF700x WiFi driver, WPA supplicant
- `esp32_devkitc_esp32_procpu.conf` — ESP32 WiFi, POSIX API

## Troubleshooting

**nRF7002-DK build fails with "nrf70.bin missing":**
Run `west blobs fetch nrf_wifi` to download the WiFi firmware blob.

**ESP32 build fails with "esptool not found":**
Run `pip install "esptool>=5.0.2"`.

**BLE connection drops after provisioning:**
This is expected. The device restarts advertising after disconnect. Reconnect to check status.

**No WiFi APs found during scan:**
Ensure the device is in range of 2.4 GHz networks. The scan only covers 2.4 GHz band.

**Auto-connect not working after reboot:**
Verify `CONFIG_WIFI_PROV_AUTO_CONNECT=y` and `CONFIG_SETTINGS_NVS=y` in prj.conf. On QEMU, credentials are only stored in-memory (no persistence).
