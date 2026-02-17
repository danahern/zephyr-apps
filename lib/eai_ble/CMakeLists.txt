# eai_ble — portable BLE GATT abstraction
zephyr_library_named(eai_ble)
zephyr_include_directories_ifdef(CONFIG_EAI_BLE include)

zephyr_library_sources_ifdef(CONFIG_EAI_BLE_BACKEND_ZEPHYR
    src/zephyr/ble.c
)
