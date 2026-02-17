# eai_settings — portable key-value store
zephyr_library_named(eai_settings)
zephyr_include_directories_ifdef(CONFIG_EAI_SETTINGS include)

zephyr_library_sources_ifdef(CONFIG_EAI_SETTINGS_BACKEND_ZEPHYR
    src/zephyr/settings.c
)
