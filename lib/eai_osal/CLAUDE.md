# eai_osal

OS abstraction layer providing portable primitives for multi-RTOS development. Three backends: Zephyr, FreeRTOS (ESP-IDF), and POSIX (Linux/macOS).

## What It Does

Wraps OS kernel primitives behind a backend-agnostic C API. Application code uses `eai_osal_*` functions; the backend (selected at compile time via Kconfig) maps them to the underlying RTOS.

**Primitives:** mutex, semaphore, thread, queue, timer, event flags, critical section, time, work queue.

## When to Use

Use this for application-level code that may need to run on multiple RTOSes. Don't use it for Zephyr-specific driver code or boot-time initialization that inherently depends on Zephyr APIs.

## How to Include

The library is auto-discovered via the workspace-level `zephyr/module.yml`. Just enable it in your prj.conf:

```conf
CONFIG_EAI_OSAL=y
CONFIG_NUM_PREEMPT_PRIORITIES=32
```

Then include the umbrella header:

```c
#include <eai_osal/eai_osal.h>
```

Or include individual headers: `<eai_osal/mutex.h>`, `<eai_osal/thread.h>`, etc.

## Kconfig Options (Zephyr/FreeRTOS)

| Option | Default | Description |
|--------|---------|-------------|
| `CONFIG_EAI_OSAL` | n | Enable the OSAL library |
| `CONFIG_EAI_OSAL_BACKEND_ZEPHYR` | y (when EAI_OSAL) | Use Zephyr kernel backend |
| `CONFIG_NUM_PREEMPT_PRIORITIES` | (board default) | Must be >= 32 (enforced via BUILD_ASSERT) |

The Zephyr backend auto-selects `CONFIG_EVENTS` and requires `CONFIG_MULTITHREADING`.

## POSIX Backend (Native Tests)

The POSIX backend enables running OSAL code natively on Linux/macOS without QEMU or hardware. Selected via `CONFIG_EAI_OSAL_BACKEND_POSIX` (set automatically by the native test CMakeLists.txt).

```bash
cd lib/eai_osal/tests/native
cmake -B build && cmake --build build
./build/osal_tests                             # 44 tests, <1s

cmake -B build-san -DENABLE_SANITIZERS=ON      # ASan + UBSan
cmake --build build-san && ./build-san/osal_tests
```

macOS compatibility notes:
- No `pthread_mutex_timedlock` — uses trylock+sleep loop
- No `sem_init` — uses mutex+condvar counting semaphore
- No `timer_create` — uses dedicated timer threads
- Condvar timeouts use `CLOCK_REALTIME` (macOS doesn't support `CLOCK_MONOTONIC` for condvars)

## API Reference

### Status Codes

All functions return `eai_osal_status_t`:
- `EAI_OSAL_OK` (0) — success
- `EAI_OSAL_ERROR` (-1) — generic error
- `EAI_OSAL_TIMEOUT` (-2) — operation timed out
- `EAI_OSAL_NO_MEMORY` (-3) — out of memory
- `EAI_OSAL_INVALID_PARAM` (-4) — NULL pointer or bad argument

Timeout constants: `EAI_OSAL_WAIT_FOREVER` (block indefinitely), `EAI_OSAL_NO_WAIT` (try once).

### Mutex

```c
eai_osal_mutex_t mtx;
eai_osal_mutex_create(&mtx);
eai_osal_mutex_lock(&mtx, EAI_OSAL_WAIT_FOREVER);
eai_osal_mutex_unlock(&mtx);
eai_osal_mutex_destroy(&mtx);
```

Supports recursive locking by the same thread. Try-lock via `timeout_ms=EAI_OSAL_NO_WAIT`.

### Semaphore

```c
eai_osal_sem_t sem;
eai_osal_sem_create(&sem, 0 /* initial */, 1 /* limit */);
eai_osal_sem_give(&sem);
eai_osal_sem_take(&sem, 100 /* ms */);
eai_osal_sem_destroy(&sem);
```

### Thread

```c
EAI_OSAL_THREAD_STACK_DEFINE(my_stack, 1024);

void my_entry(void *arg) { /* ... */ }

eai_osal_thread_t thread;
eai_osal_thread_create(&thread, "my_thread", my_entry, NULL,
                       my_stack, EAI_OSAL_THREAD_STACK_SIZEOF(my_stack), 10);
eai_osal_thread_join(&thread, EAI_OSAL_WAIT_FOREVER);
eai_osal_thread_sleep(100);
eai_osal_thread_yield();
```

Priority: 0-31, higher number = higher priority. Maps to Zephyr preemptive priorities (31-prio).

### Queue

```c
static uint8_t __aligned(4) buf[4 * sizeof(int)];
eai_osal_queue_t queue;
eai_osal_queue_create(&queue, sizeof(int), 4, buf);

int msg = 42;
eai_osal_queue_send(&queue, &msg, EAI_OSAL_WAIT_FOREVER);
eai_osal_queue_recv(&queue, &msg, 100);
eai_osal_queue_destroy(&queue);
```

Caller provides a statically-allocated buffer. FIFO order. Buffer must be aligned to word boundary.

### Timer

```c
void my_cb(void *arg) { /* ISR context on Zephyr! */ }

eai_osal_timer_t timer;
eai_osal_timer_create(&timer, my_cb, NULL);
eai_osal_timer_start(&timer, 100 /* initial_ms */, 50 /* period_ms, 0=one-shot */);
eai_osal_timer_stop(&timer);
bool running = eai_osal_timer_is_running(&timer);
eai_osal_timer_destroy(&timer);
```

Timer callbacks execute in ISR context (Zephyr). Keep them short and ISR-safe.

### Event Flags

```c
eai_osal_event_t event;
eai_osal_event_create(&event);
eai_osal_event_set(&event, 0x03);  /* OR bits into state */

uint32_t actual;
eai_osal_event_wait(&event, 0x0F, false /* wait_any */, &actual, 100);
eai_osal_event_wait(&event, 0x03, true /* wait_all */, &actual, 100);
eai_osal_event_clear(&event, 0x01);
eai_osal_event_destroy(&event);
```

Bits are not auto-cleared on wait. Use `eai_osal_event_clear` explicitly.

### Critical Section

```c
eai_osal_critical_key_t key = eai_osal_critical_enter();
/* IRQs disabled */
eai_osal_critical_exit(key);
```

Supports nesting. Uses IRQ lock/unlock on Zephyr.

### Time

```c
uint32_t ms = eai_osal_time_get_ms();
uint64_t ticks = eai_osal_time_get_ticks();
uint32_t converted = eai_osal_time_ticks_to_ms(ticks);
```

### Work Queue

```c
/* Simple work — runs on system work queue */
void my_handler(void *arg) { /* thread context */ }

eai_osal_work_t work;
eai_osal_work_init(&work, my_handler, NULL);
eai_osal_work_submit(&work);

/* Delayed work — runs after a delay */
eai_osal_dwork_t dwork;
eai_osal_dwork_init(&dwork, my_handler, NULL);
eai_osal_dwork_submit(&dwork, 200 /* ms */);
eai_osal_dwork_cancel(&dwork);

/* Custom work queue — dedicated thread */
EAI_OSAL_THREAD_STACK_DEFINE(wq_stack, 1024);
eai_osal_workqueue_t wq;
eai_osal_workqueue_create(&wq, "my_wq", wq_stack,
                          EAI_OSAL_THREAD_STACK_SIZEOF(wq_stack), 10);
eai_osal_work_submit_to(&work, &wq);
eai_osal_dwork_submit_to(&dwork, &wq, 100);
```

Work callbacks execute in thread context (system work queue thread or custom queue thread). Custom work queues must be static — the thread persists for the lifetime of the queue.

## Architecture

### Backend Type Dispatch

`include/eai_osal/types.h` defines status codes and common typedefs, then conditionally includes the backend-specific types via a relative path:

```
include/eai_osal/types.h  →  #include "../../src/zephyr/types.h"
```

This gives each backend control over the concrete type definitions (e.g., `eai_osal_mutex_t` wraps `struct k_mutex` on Zephyr).

### Internal Helpers

`src/zephyr/internal.h` provides `osal_timeout()` (ms → k_timeout_t) and `osal_status()` (Zephyr errno → OSAL status code) used by all backend implementations.

### Test Setup

Tests use raw Zephyr threads for contention scenarios (don't test OSAL with itself). The test doesn't need `ZEPHYR_EXTRA_MODULES` since eai_osal is auto-discovered via the workspace module.

## Testing

**Zephyr (QEMU):**
```
zephyr-build.build(app="lib/eai_osal/tests", board="qemu_cortex_m3", pristine=true)
```

**Native (POSIX):**
```bash
cd lib/eai_osal/tests/native && cmake -B build && cmake --build build && ./build/osal_tests
```

44 tests across 9 suites: mutex (6), semaphore (5), thread (4), queue (5), timer (5), event (5), critical (2), time (3), work (9).

## Files

```
lib/eai_osal/
├── CMakeLists.txt
├── Kconfig
├── CLAUDE.md
├── manifest.yml
├── zephyr/module.yml
├── include/eai_osal/       # Public API headers
│   ├── eai_osal.h          # Umbrella include
│   ├── types.h             # Status codes + backend dispatch
│   ├── mutex.h, semaphore.h, thread.h, queue.h
│   ├── timer.h, event.h, critical.h, time.h, workqueue.h
├── src/zephyr/             # Zephyr backend
│   ├── types.h             # k_mutex → eai_osal_mutex_t wrappers
│   ├── internal.h          # Timeout/status conversion
│   ├── mutex.c, semaphore.c, thread.c, queue.c
│   ├── timer.c, event.c, critical.c, time.c, workqueue.c
├── src/freertos/           # FreeRTOS backend (ESP-IDF)
│   ├── types.h, internal.h
│   ├── mutex.c, semaphore.c, thread.c, queue.c
│   ├── timer.c, event.c, critical.c, time.c, workqueue.c
├── src/posix/              # POSIX backend (Linux/macOS)
│   ├── types.h             # pthread_mutex_t → eai_osal_mutex_t wrappers
│   ├── internal.h          # osal_timespec() timeout helper
│   ├── mutex.c, semaphore.c, thread.c, queue.c
│   ├── timer.c, event.c, critical.c, time.c, workqueue.c
└── tests/
    ├── CMakeLists.txt, prj.conf, testcase.yaml
    ├── src/main.c                  # Zephyr ztest (44 tests)
    └── native/                     # Standalone native tests
        ├── CMakeLists.txt          # cmake -B build && cmake --build build
        ├── main.c                  # Unity (44 tests, ported from ESP-IDF)
        └── unity/                  # Vendored Unity v2.6.0
```
