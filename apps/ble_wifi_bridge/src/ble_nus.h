/*
 * BLE NUS Module Header
 */

#ifndef BLE_NUS_H
#define BLE_NUS_H

#include <zephyr/kernel.h>
#include <stdbool.h>

/* Callback type for received BLE data */
typedef void (*ble_nus_rx_cb_t)(const uint8_t *data, uint16_t len);

/**
 * @brief Initialize BLE NUS module
 *
 * @param rx_callback Callback function for received data
 * @return 0 on success, negative errno on failure
 */
int ble_nus_init(ble_nus_rx_cb_t rx_callback);

/**
 * @brief Start BLE advertising
 *
 * @return 0 on success, negative errno on failure
 */
int ble_nus_start_advertising(void);

/**
 * @brief Send data over BLE NUS
 *
 * @param data Data to send
 * @param len Length of data
 * @return Number of bytes sent on success, negative errno on failure
 */
int ble_nus_send(const uint8_t *data, uint16_t len);

/**
 * @brief Check if BLE is connected
 *
 * @return true if connected, false otherwise
 */
bool ble_nus_is_connected(void);

#endif /* BLE_NUS_H */
