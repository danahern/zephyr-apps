# eai_ble — Portable BLE GATT Abstraction

Declarative BLE GATT service definition with backends for Zephyr BT, NimBLE (ESP-IDF), and a POSIX stub for testing. Consumer describes a service once; backend handles platform-specific registration.

## What It Does

- `eai_ble_init(cbs)` — Initialize BLE subsystem with optional connect/disconnect callbacks
- `eai_ble_gatt_register(svc)` — Register a GATT service (max 8 characteristics)
- `eai_ble_adv_start(name)` — Start BLE advertising with device name
- `eai_ble_adv_stop()` — Stop advertising
- `eai_ble_notify(char_index, data, len)` — Send notification on a characteristic
- `eai_ble_is_connected()` — Check if a peer is connected

## When to Use

Any app or library that needs a BLE GATT service on both Zephyr and ESP-IDF. Use instead of calling Zephyr BT or NimBLE APIs directly.

## How to Include

1. **Zephyr** — Add to `prj.conf`:
   ```
   CONFIG_BT=y
   CONFIG_BT_PERIPHERAL=y
   CONFIG_BT_GATT_DYNAMIC_DB=y
   CONFIG_EAI_BLE=y
   ```

2. **ESP-IDF** — Add as component, compile `src/freertos/ble.c`, link NimBLE.

3. **Native/POSIX** — Compile `src/posix/ble.c` with `CONFIG_EAI_BLE_BACKEND_POSIX` and `EAI_BLE_TEST` defined.

4. **Source code**:
   ```c
   #include <eai_ble/eai_ble.h>

   static const struct eai_ble_char chars[] = {
       {
           .uuid = EAI_BLE_UUID128_INIT(0xa0e4f2b0, 0x0002, 0x4c9a,
                                        0xb000, 0xd0e6a7b8c9d0ULL),
           .properties = EAI_BLE_PROP_WRITE,
           .on_write = my_write_handler,
       },
       {
           .uuid = EAI_BLE_UUID128_INIT(0xa0e4f2b0, 0x0003, 0x4c9a,
                                        0xb000, 0xd0e6a7b8c9d0ULL),
           .properties = EAI_BLE_PROP_NOTIFY,
       },
   };

   static const struct eai_ble_service svc = {
       .uuid = EAI_BLE_UUID128_INIT(0xa0e4f2b0, 0x0001, 0x4c9a,
                                    0xb000, 0xd0e6a7b8c9d0ULL),
       .chars = chars,
       .char_count = 2,
   };

   eai_ble_init(&(struct eai_ble_callbacks){
       .on_connect = on_connect,
       .on_disconnect = on_disconnect,
   });
   eai_ble_gatt_register(&svc);
   eai_ble_adv_start("MyDevice");
   ```

## Key Design Decisions

- **Char index, not handle.** Consumer references characteristics by index (0..N-1). Backend maps to platform-specific handles.
- **UUID in little-endian.** Matches BLE wire format. NimBLE uses directly; Zephyr backend copies directly (both use LE internally).
- **Write callback flattens data.** Both backends extract raw bytes before calling the callback — consumer never sees mbufs or bt_conn.
- **Single service.** v1 supports one registered service.
- **Auto-restart advertising on disconnect.** Baked into both real backends.

## Kconfig Options

| Option | Depends On | Description |
|--------|-----------|-------------|
| `CONFIG_EAI_BLE` | - | Enable library |
| `CONFIG_EAI_BLE_BACKEND_ZEPHYR` | BT, BT_PERIPHERAL, BT_GATT_DYNAMIC_DB | Zephyr BT backend |

## Return Values

| Return | Meaning |
|--------|---------|
| `0` | Success |
| `-EINVAL` | NULL service, too many chars, bad args |
| `-ENOTCONN` | No connected peer (notify) |

## Backends

**Zephyr** — Uses `bt_gatt_service_register()` with `CONFIG_BT_GATT_DYNAMIC_DB`. Builds `bt_gatt_attr[]` at register time. Static arrays sized for 8 chars (25 attrs max). CCC descriptors auto-added for notify characteristics.

**NimBLE** — Builds `ble_gatt_svc_def`/`ble_gatt_chr_def` arrays. Registration via `ble_gatts_count_cfg()` + `ble_gatts_add_svcs()`. NimBLE host task started at register time. CCC handled automatically by NimBLE.

**POSIX** — State-tracking stub. All operations succeed if state is valid. `eai_ble_test_set_connected(bool)` simulates connection events for testing.

## Gotchas

- **`BT_GATT_DYNAMIC_DB` required** for Zephyr runtime service registration.
- **Max 8 characteristics** per service. Exceeding returns -EINVAL.
- **NimBLE sync callback** fires asynchronously after init. Don't call `adv_start` before `gatt_register`.
- **Zephyr 4.x CCC API** uses `bt_gatt_ccc_managed_user_data` (not `_bt_gatt_ccc`).

## Testing

**Native tests** (POSIX backend): 9 tests
```bash
cd tests/native && cmake -B build && cmake --build build && ./build/eai_ble_tests
```

**Zephyr build-only**: Compile verification on nrf52840dk
```
zephyr-build.build(app="firmware/lib/eai_ble/tests", board="nrf52840dk/nrf52840", pristine=true)
```

## Files

```
lib/eai_ble/
  include/eai_ble/eai_ble.h     # Public API (6 functions, UUID macro)
  src/zephyr/ble.c               # Zephyr BT backend
  src/freertos/ble.c             # NimBLE backend
  src/posix/ble.c                # Stub backend
  tests/native/                  # 9 Unity tests (POSIX)
  tests/src/main.c               # Zephyr build-only test
  CMakeLists.txt
  Kconfig
```
