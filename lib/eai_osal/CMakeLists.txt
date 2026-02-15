zephyr_library_named(eai_osal)

zephyr_include_directories_ifdef(CONFIG_EAI_OSAL include)

zephyr_library_sources_ifdef(CONFIG_EAI_OSAL_BACKEND_ZEPHYR
    src/zephyr/mutex.c
    src/zephyr/semaphore.c
    src/zephyr/thread.c
    src/zephyr/queue.c
    src/zephyr/timer.c
    src/zephyr/event.c
    src/zephyr/critical.c
    src/zephyr/time.c
    src/zephyr/workqueue.c
)
