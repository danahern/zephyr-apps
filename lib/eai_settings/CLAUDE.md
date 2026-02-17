# eai_settings — Portable Key-Value Store

Persistent key-value store with backends for Zephyr Settings, ESP-IDF NVS, and POSIX file-based storage.

## What It Does

- `eai_settings_set(key, data, len)` — store a blob
- `eai_settings_get(key, buf, buf_len, &actual_len)` — read a blob (reports truncation via actual_len)
- `eai_settings_delete(key)` — remove a key
- `eai_settings_exists(key)` — check if a key exists
- `eai_settings_init()` — initialize the backend

## When to Use

Any shared library or app that needs persistent storage. Use instead of calling Zephyr Settings or ESP-IDF NVS directly.

## Key Format

Keys use `"namespace/key"` format — first `/` separates namespace from key name.

```c
eai_settings_set("wifi_prov/ssid", ssid, strlen(ssid));
eai_settings_get("wifi_prov/ssid", buf, sizeof(buf), &actual);
```

## How to Include

1. **Zephyr** — Add to `prj.conf`:
   ```
   CONFIG_EAI_SETTINGS=y
   CONFIG_SETTINGS=y
   CONFIG_NVS=y
   CONFIG_SETTINGS_NVS=y
   CONFIG_FLASH=y
   CONFIG_FLASH_MAP=y
   ```

2. **ESP-IDF** — Add as component, link `eai_settings` with `CONFIG_EAI_SETTINGS_BACKEND_FREERTOS` defined.

3. **Native/POSIX** — Compile `src/posix/settings.c` with `CONFIG_EAI_SETTINGS_BACKEND_POSIX` and `EAI_SETTINGS_BASE_PATH` defined.

4. **Source code**:
   ```c
   #include <eai_settings/eai_settings.h>

   eai_settings_init();
   eai_settings_set("app/config", data, sizeof(data));
   ```

## Kconfig Options

| Option | Default | Description |
|--------|---------|-------------|
| `CONFIG_EAI_SETTINGS` | n | Enable eai_settings library |
| `CONFIG_EAI_SETTINGS_BACKEND_ZEPHYR` | y (if SETTINGS) | Zephyr Settings backend |

## Return Values

| Return | Meaning |
|--------|---------|
| `0` | Success |
| `-EINVAL` | NULL key, NULL data, or zero length |
| `-ENOENT` | Key not found |
| `-EIO` | Backend I/O error |

## Backends

**Zephyr** — All keys stored under `"eai"` prefix. `set("wifi_prov/ssid")` → `settings_save_one("eai/wifi_prov/ssid")`. Thread-safe via K_MUTEX.

**FreeRTOS/ESP-IDF** — Splits key on first `/` → NVS namespace + NVS key. Namespace and key limited to 15 chars each. `nvs_flash_init()` with erase-on-corrupt recovery.

**POSIX** — File-based at `<EAI_SETTINGS_BASE_PATH>/<namespace>/<key>`. Thread-safe via pthread_mutex. Default base path: `/tmp/eai_settings`.

## Gotchas

- **NVS sector size limits large values.** On mps2/an385 (1KB sectors), max value ~900 bytes. Real hardware typically has 4KB sectors.
- **ESP-IDF NVS namespace/key limit**: 15 chars each.
- **Zephyr `settings_load_subtree` loads ALL keys** under the prefix to find one key. O(n) but fine for <50 keys.
- **`actual_len` reports true data size even on truncated reads** — compare with `buf_len` to detect truncation.

## Testing

**Native tests** (POSIX backend): 14 tests
```bash
cd tests/native && cmake -B build && cmake --build build && ./build/eai_settings_tests
```

**Zephyr tests**: 14 tests on mps2/an385
```
run_tests(board="mps2/an385", path="lib/eai_settings")
```

## Files

```
lib/eai_settings/
  include/eai_settings/eai_settings.h  # Public API (5 functions)
  src/zephyr/settings.c                # Zephyr Settings backend
  src/freertos/settings.c              # ESP-IDF NVS backend
  src/posix/settings.c                 # File-based backend
  tests/native/                        # 14 Unity tests (POSIX)
  tests/src/main.c                     # 14 ztest tests (Zephyr)
  CMakeLists.txt
  Kconfig
```
