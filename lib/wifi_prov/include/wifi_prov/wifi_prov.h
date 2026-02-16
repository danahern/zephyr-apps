#ifndef WIFI_PROV_H
#define WIFI_PROV_H

#include <wifi_prov/wifi_prov_types.h>

/**
 * @brief Initialize the WiFi provisioning subsystem.
 *
 * Initializes credential store, state machine, BLE, and WiFi modules.
 * If stored credentials exist and auto-connect is enabled, begins
 * connecting to WiFi immediately.
 *
 * @return 0 on success, negative errno on failure.
 */
int wifi_prov_init(void);

/**
 * @brief Start BLE advertising for provisioning.
 *
 * Call after wifi_prov_init(). Device becomes discoverable over BLE.
 *
 * @return 0 on success, negative errno on failure.
 */
int wifi_prov_start(void);

/**
 * @brief Factory reset: erase credentials, disconnect, return to idle.
 *
 * @return 0 on success, negative errno on failure.
 */
int wifi_prov_factory_reset(void);

/**
 * @brief Get current provisioning state.
 *
 * @return Current state.
 */
enum wifi_prov_state wifi_prov_get_state(void);

/**
 * @brief Get WiFi IP address. Valid only in CONNECTED state.
 *
 * @param ip_addr Output buffer for IPv4 address (4 bytes).
 * @return 0 on success, -ENOTCONN if not connected.
 */
int wifi_prov_get_ip(uint8_t ip_addr[4]);

/* --- Credential store API --- */

/**
 * @brief Store WiFi credentials to persistent storage.
 * @return 0 on success, negative errno on failure.
 */
int wifi_prov_cred_store(const struct wifi_prov_cred *cred);

/**
 * @brief Load WiFi credentials from persistent storage.
 * @return 0 on success, negative errno on failure.
 */
int wifi_prov_cred_load(struct wifi_prov_cred *cred);

/**
 * @brief Erase stored WiFi credentials.
 * @return 0 on success, negative errno on failure.
 */
int wifi_prov_cred_erase(void);

/**
 * @brief Check if WiFi credentials exist in storage.
 * @return true if credentials exist.
 */
bool wifi_prov_cred_exists(void);

/* --- State machine API --- */

typedef void (*wifi_prov_state_cb_t)(enum wifi_prov_state old_state,
				     enum wifi_prov_state new_state);

void wifi_prov_sm_init(wifi_prov_state_cb_t callback);
enum wifi_prov_state wifi_prov_sm_get_state(void);
int wifi_prov_sm_process_event(enum wifi_prov_event event);

/* --- BLE API --- */

int wifi_prov_ble_init(void);
int wifi_prov_ble_start_advertising(void);
void wifi_prov_ble_set_callbacks(void (*on_scan_trigger)(void),
				 void (*on_credentials)(const struct wifi_prov_cred *),
				 void (*on_factory_reset)(void));
int wifi_prov_ble_notify_scan_result(const struct wifi_prov_scan_result *result);
int wifi_prov_ble_notify_status(enum wifi_prov_state state,
				const uint8_t ip[4]);

/* --- WiFi API --- */

typedef void (*wifi_prov_wifi_state_cb_t)(bool connected);
typedef void (*wifi_prov_scan_result_cb_t)(const struct wifi_prov_scan_result *result);

int wifi_prov_wifi_init(wifi_prov_wifi_state_cb_t state_cb);
int wifi_prov_wifi_scan(wifi_prov_scan_result_cb_t result_cb);
int wifi_prov_wifi_connect(const struct wifi_prov_cred *cred);
int wifi_prov_wifi_disconnect(void);
int wifi_prov_wifi_get_ip(uint8_t ip_addr[4]);
bool wifi_prov_wifi_is_connected(void);

#endif /* WIFI_PROV_H */
