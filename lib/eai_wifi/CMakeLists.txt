# eai_wifi — portable WiFi connection manager
zephyr_library_named(eai_wifi)
zephyr_include_directories_ifdef(CONFIG_EAI_WIFI include)

zephyr_library_sources_ifdef(CONFIG_EAI_WIFI_BACKEND_ZEPHYR
    src/zephyr/wifi.c
)
