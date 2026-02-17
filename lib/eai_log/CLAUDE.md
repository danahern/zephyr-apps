# eai_log — Portable Logging Macros

Header-only logging abstraction. Macros compile to the native logging system on each platform with zero overhead.

## What It Does

- `EAI_LOG_INF/ERR/WRN/DBG(fmt, ...)` — log at the given level
- `EAI_LOG_MODULE_REGISTER(name, level)` — register a module (one per .c file)
- `EAI_LOG_MODULE_DECLARE(name, level)` — declare in multi-file modules
- Compile-time level filtering — messages below the registered level are eliminated

## When to Use

Any shared library that needs logging. Use `EAI_LOG_*` instead of Zephyr `LOG_*` or ESP-IDF `ESP_LOG*` to keep libraries portable.

## How to Include

1. **Zephyr** — Add to `prj.conf`:
   ```
   CONFIG_EAI_LOG=y
   ```
   Backend auto-selects Zephyr (depends on `CONFIG_LOG`).

2. **ESP-IDF** — Add compile definition in CMakeLists.txt:
   ```cmake
   target_compile_definitions(${COMPONENT_LIB} PUBLIC CONFIG_EAI_LOG_BACKEND_FREERTOS)
   ```

3. **Native/POSIX** — Add compile definition:
   ```cmake
   target_compile_definitions(my_target PRIVATE CONFIG_EAI_LOG_BACKEND_POSIX)
   ```

4. **Source code**:
   ```c
   #include <eai_log/eai_log.h>
   EAI_LOG_MODULE_REGISTER(my_module, EAI_LOG_LEVEL_INF);

   void foo(void) {
       EAI_LOG_INF("Hello %s", "world");
       EAI_LOG_ERR("Failed: %d", -1);
   }
   ```

## Kconfig Options

| Option | Default | Description |
|--------|---------|-------------|
| `CONFIG_EAI_LOG` | n | Enable eai_log library |
| `CONFIG_EAI_LOG_BACKEND_ZEPHYR` | y (if LOG) | Zephyr LOG_* passthrough |

## Log Levels

| Level | Value | Macro |
|-------|-------|-------|
| None | 0 | `EAI_LOG_LEVEL_NONE` |
| Error | 1 | `EAI_LOG_LEVEL_ERR` |
| Warning | 2 | `EAI_LOG_LEVEL_WRN` |
| Info | 3 | `EAI_LOG_LEVEL_INF` |
| Debug | 4 | `EAI_LOG_LEVEL_DBG` |

Values match Zephyr's `LOG_LEVEL_*` constants.

## Backends

| Backend | `REGISTER` expands to | `LOG_INF(...)` expands to |
|---------|----------------------|--------------------------|
| Zephyr | `LOG_MODULE_REGISTER(name, level)` | `LOG_INF(...)` |
| FreeRTOS | `static const char *TAG = "name"` | `ESP_LOGI(TAG, ...)` |
| POSIX | `static const char *TAG + enum level` | `fprintf(stderr, "[INF] name: ...")` |

## Gotchas

- `EAI_LOG_MODULE_REGISTER` must be at **file scope** — Zephyr defines a static struct.
- Never use both `REGISTER` and `DECLARE` in the same file — causes duplicate variables on ESP-IDF/POSIX.
- POSIX backend uses `##__VA_ARGS__` GNU extension (GCC/Clang only).

## Testing

Native tests (POSIX backend): 5 tests
```bash
cd tests/native && cmake -B build && cmake --build build && ./build/eai_log_tests
```

## Files

```
lib/eai_log/
  include/eai_log/eai_log.h    # Public API + backend dispatch
  src/zephyr.h                  # Zephyr LOG_* passthrough
  src/freertos.h                # ESP-IDF ESP_LOG* wrapper
  src/posix.h                   # fprintf(stderr) with level filtering
  tests/native/                 # 5 Unity tests for POSIX backend
  CMakeLists.txt
  Kconfig
```
