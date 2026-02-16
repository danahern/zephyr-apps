# ESP-IDF platform-specific wifi provisioning implementations

idf_component_register(
    SRCS
        "wifi_prov_ble_esp.c"
        "wifi_prov_wifi_esp.c"
        "wifi_prov_cred_esp.c"
        "wifi_prov_esp.c"
    INCLUDE_DIRS "."
    REQUIRES
        bt
        esp_wifi
        esp_event
        esp_netif
        nvs_flash
        wifi_prov_common
        eai_osal
)
