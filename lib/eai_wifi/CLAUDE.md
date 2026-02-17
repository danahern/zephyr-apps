# eai_wifi — Portable WiFi Connection Manager

WiFi scan/connect/disconnect abstraction with backends for Zephyr net_mgmt, ESP-IDF esp_wifi, and a POSIX stub for testing.

## What It Does

- `eai_wifi_init()` — Initialize WiFi subsystem
- `eai_wifi_set_event_callback(cb)` — Register connect/disconnect/failure callback
- `eai_wifi_scan(on_result, on_done)` — Start AP scan with per-result + done callbacks
- `eai_wifi_connect(ssid, ssid_len, psk, psk_len, sec)` — Connect to AP (raw bytes, not null-terminated)
- `eai_wifi_disconnect()` — Disconnect from current AP
- `eai_wifi_get_state()` — Get current state (DISCONNECTED/SCANNING/CONNECTING/CONNECTED)
- `eai_wifi_get_ip(ip)` — Get IPv4 address (4 bytes)

## When to Use

Any app or library that needs WiFi scan/connect on both Zephyr and ESP-IDF. Use instead of calling `net_mgmt` or `esp_wifi` directly. Designed as drop-in for `wifi_prov_wifi_*`.

## How to Include

1. **Zephyr** — Add to `prj.conf`:
   ```
   CONFIG_WIFI=y
   CONFIG_NETWORKING=y
   CONFIG_NET_MGMT=y
   CONFIG_NET_MGMT_EVENT=y
   CONFIG_NET_IPV4=y
   CONFIG_NET_DHCPV4=y
   CONFIG_NET_L2_WIFI_MGMT=y
   CONFIG_EAI_WIFI=y
   ```

2. **ESP-IDF** — Add as component, compile `src/freertos/wifi.c`, link esp_wifi + esp_event + esp_netif.

3. **Native/POSIX** — Compile `src/posix/wifi.c` with `CONFIG_EAI_WIFI_BACKEND_POSIX` and `EAI_WIFI_TEST` defined.

4. **Source code**:
   ```c
   #include <eai_wifi/eai_wifi.h>

   void on_event(enum eai_wifi_event event) {
       if (event == EAI_WIFI_EVT_CONNECTED) { /* ready */ }
   }

   eai_wifi_init();
   eai_wifi_set_event_callback(on_event);
   eai_wifi_connect(ssid, ssid_len, psk, psk_len, EAI_WIFI_SEC_WPA2_PSK);
   ```

## Key Design Decisions

- **connect() takes raw bytes + lengths.** Matches `wifi_prov_cred` structure where SSID/PSK are `uint8_t[]` with lengths, not null-terminated strings.
- **EVT_CONNECTED fires only after IP obtained.** Both backends wait for DHCP before signaling connected.
- **DHCP handled internally.** Zephyr calls `net_dhcpv4_start()`. ESP-IDF uses automatic esp_netif DHCP.
- **Power management handled internally.** ESP-IDF backend calls `esp_wifi_set_ps(WIFI_PS_NONE)`.
- **4 states, not 6.** No PROVISIONING/SCAN_COMPLETE — those belong to the provisioning layer.
- **Zephyr PSK covers WPA+WPA2.** `WIFI_SECURITY_TYPE_PSK` handles both; AP negotiates.
- **ESP-IDF disconnect ambiguity.** Backend tracks `current_state` to differentiate auth failure (CONNECTING→CONNECT_FAILED) from real disconnect (CONNECTED→DISCONNECTED).

## Kconfig Options

| Option | Depends On | Description |
|--------|-----------|-------------|
| `CONFIG_EAI_WIFI` | - | Enable library |
| `CONFIG_EAI_WIFI_BACKEND_ZEPHYR` | WIFI, NETWORKING, NET_MGMT, NET_MGMT_EVENT | Zephyr net_mgmt backend |

## Return Values

| Return | Meaning |
|--------|---------|
| `0` | Success |
| `-EINVAL` | NULL SSID, zero length, NULL on_result callback |
| `-ENOTCONN` | get_ip() when not connected |
| `-ENODEV` | No network interface (Zephyr) |

## Backends

**Zephyr** — Uses `net_mgmt` callbacks for `WIFI_SCAN_RESULT`, `WIFI_SCAN_DONE`, `WIFI_CONNECT_RESULT`, `WIFI_DISCONNECT_RESULT`, `IPV4_ADDR_ADD`. DHCP started explicitly via `net_dhcpv4_start()`.

**ESP-IDF** — Single `wifi_event_handler` for `WIFI_EVENT` and `IP_EVENT`. Scan results collected in batch via `esp_wifi_scan_get_ap_records()`, then iterated. Power save disabled in init.

**POSIX** — State-tracking stub with 6 test helpers: `test_reset`, `test_inject_scan_result`, `test_complete_scan`, `test_set_connected`, `test_set_disconnected`, `test_set_connect_failed`.

## Gotchas

- **nrf7002dk needs board overlay** with `CONFIG_WIFI_NRF70=y`, `CONFIG_WIFI_NM=y`, `CONFIG_WIFI_NM_WPA_SUPPLICANT=y`.
- **Build-only test needs `CONFIG_TEST_RANDOM_GENERATOR=y`** — networking stack requires entropy, and the nRF entropy driver doesn't compile without BT pulling it in.
- **ESP-IDF modem sleep** — Must call `esp_wifi_set_ps(WIFI_PS_NONE)` or incoming TCP/ping is blocked.

## Testing

**Native tests** (POSIX backend): 17 tests
```bash
cd tests/native && cmake -B build && cmake --build build && ./build/eai_wifi_tests
```

**Zephyr build-only**: Compile verification on nrf7002dk
```
zephyr-build.build(app="firmware/lib/eai_wifi/tests", board="nrf7002dk/nrf5340/cpuapp", pristine=true)
```

## Files

```
lib/eai_wifi/
  include/eai_wifi/eai_wifi.h     # Public API (8 functions, test helpers)
  src/zephyr/wifi.c                # Zephyr net_mgmt backend
  src/freertos/wifi.c              # ESP-IDF esp_wifi backend
  src/posix/wifi.c                 # Stub backend
  tests/native/                    # 17 Unity tests (POSIX)
  tests/src/main.c                 # Zephyr build-only test
  tests/boards/                    # Board overlays (nrf7002dk)
  CMakeLists.txt
  Kconfig
```
