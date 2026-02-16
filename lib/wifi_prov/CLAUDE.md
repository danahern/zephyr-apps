# WiFi Provisioning Library

Shared library for WiFi provisioning over BLE. Provides a custom GATT service, WiFi management, credential persistence, and a state machine for the provisioning flow.

## Architecture

```
wifi_prov.c          -- Orchestrator: wires BLE, WiFi, state machine, credential store
wifi_prov_ble.c      -- BLE GATT service (5 characteristics)
wifi_prov_wifi.c     -- WiFi scan/connect via net_mgmt
wifi_prov_sm.c       -- State machine (6 states, 8 events)
wifi_prov_cred.c     -- Credential persistence via Zephyr Settings
wifi_prov_msg.c      -- Wire format encode/decode
wifi_prov_stub.c     -- LOG_MODULE_REGISTER only
```

## Public API

```c
#include <wifi_prov/wifi_prov.h>

/* Top-level (apps call these two) */
int wifi_prov_init(void);     /* Init all subsystems, auto-connect if creds exist */
int wifi_prov_start(void);    /* Start BLE advertising */

/* Query */
enum wifi_prov_state wifi_prov_get_state(void);
int wifi_prov_get_ip(uint8_t ip_addr[4]);

/* Actions */
int wifi_prov_factory_reset(void);

/* Lower-level (used internally or for testing) */
int wifi_prov_cred_store(const struct wifi_prov_cred *cred);
int wifi_prov_cred_load(struct wifi_prov_cred *cred);
int wifi_prov_cred_erase(void);
bool wifi_prov_cred_exists(void);

void wifi_prov_sm_init(wifi_prov_state_cb_t callback);
enum wifi_prov_state wifi_prov_sm_get_state(void);
int wifi_prov_sm_process_event(enum wifi_prov_event event);
```

## Kconfig Options

| Option | Depends On | Description |
|--------|-----------|-------------|
| `CONFIG_WIFI_PROV` | - | Enable library |
| `CONFIG_WIFI_PROV_BLE` | BT, BT_PERIPHERAL | BLE GATT service |
| `CONFIG_WIFI_PROV_WIFI` | WIFI, NETWORKING | WiFi scan/connect |
| `CONFIG_WIFI_PROV_CRED` | SETTINGS | Credential persistence |
| `CONFIG_WIFI_PROV_AUTO_CONNECT` | WIFI_PROV_WIFI, WIFI_PROV_CRED | Auto-connect on boot |

## BLE GATT Service

UUID base: `a0e4f2b0-XXXX-4c9a-b000-d0e6a7b8c9d0`

| Characteristic | UUID suffix | Properties | Purpose |
|---------------|-------------|------------|---------|
| Scan Trigger | 0002 | Write | Write any byte to start AP scan |
| Scan Results | 0003 | Notify | Streams discovered APs |
| Credentials | 0004 | Write | Receive SSID + PSK + security |
| Status | 0005 | Read, Notify | Connection state + IP |
| Factory Reset | 0006 | Write | Write 0xFF to reset |

## Wire Formats

**Scan Result:** `[ssid_len:1][ssid:N][rssi:1][security:1][channel:1]`

**Credentials:** `[ssid_len:1][ssid:N][psk_len:1][psk:N][security:1]`

**Status:** `[state:1][ip_addr:4]`

Security values: 0=Open, 1=WPA-PSK, 2=WPA2-PSK, 3=WPA2-PSK-SHA256, 4=WPA3-SAE

## State Machine

```
IDLE --SCAN_TRIGGER--> SCANNING --SCAN_DONE--> SCAN_COMPLETE
IDLE --CREDENTIALS_RX--> PROVISIONING (direct provision without scan)
SCAN_COMPLETE --CREDENTIALS_RX--> PROVISIONING
PROVISIONING --WIFI_CONNECTING--> CONNECTING
CONNECTING --WIFI_CONNECTED--> CONNECTED
CONNECTING --WIFI_FAILED--> IDLE
CONNECTED --WIFI_DISCONNECTED--> IDLE
CONNECTED --SCAN_TRIGGER--> SCANNING (re-scan while connected)
* --FACTORY_RESET--> IDLE (from any state)
```

## Testing

22 unit tests on `qemu_cortex_m3`:
- 5 credential store tests
- 8 message encode/decode tests
- 9 state machine transition tests

```bash
# Via twister (recommended)
python3 zephyr/scripts/twister -T zephyr-apps/lib/wifi_prov -p qemu_cortex_m3 -v

# Note: zephyr-build.run_tests() MCP may fail due to pyenv PATH issues.
# Use twister directly with clean PATH if needed.
```

## Build

The orchestrator (`wifi_prov.c`) is only built when both `CONFIG_WIFI_PROV_BLE` and `CONFIG_WIFI_PROV_WIFI` are enabled. Individual modules can be used independently (e.g., cred store + msg for testing on QEMU).
