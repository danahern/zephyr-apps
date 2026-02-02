/*
 * WiFi Manager Header
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <zephyr/kernel.h>
#include <stdbool.h>

/* Callback type for WiFi connection state changes */
typedef void (*wifi_state_cb_t)(bool connected);

/**
 * @brief Initialize WiFi manager
 *
 * @param state_callback Callback for connection state changes
 * @return 0 on success, negative errno on failure
 */
int wifi_manager_init(wifi_state_cb_t state_callback);

/**
 * @brief Connect to configured WiFi network
 *
 * @return 0 on success, negative errno on failure
 */
int wifi_manager_connect(void);

/**
 * @brief Disconnect from WiFi network
 *
 * @return 0 on success, negative errno on failure
 */
int wifi_manager_disconnect(void);

/**
 * @brief Check if WiFi is connected
 *
 * @return true if connected, false otherwise
 */
bool wifi_manager_is_connected(void);

#endif /* WIFI_MANAGER_H */
