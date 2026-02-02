/*
 * Bridge Module Header
 *
 * Handles bidirectional data transfer between BLE and TCP.
 */

#ifndef BRIDGE_H
#define BRIDGE_H

#include <zephyr/kernel.h>

/**
 * @brief Initialize the bridge module
 *
 * Sets up message queues and callbacks for BLE/TCP bridging.
 *
 * @return 0 on success, negative errno on failure
 */
int bridge_init(void);

/**
 * @brief Start the bridge processing
 *
 * Begins forwarding data between BLE and TCP.
 * Should be called after both BLE and TCP are connected.
 */
void bridge_start(void);

/**
 * @brief Stop the bridge processing
 */
void bridge_stop(void);

/**
 * @brief Queue data received from BLE to be sent to TCP
 *
 * @param data Data buffer
 * @param len Data length
 * @return 0 on success, negative errno on failure
 */
int bridge_queue_ble_to_tcp(const uint8_t *data, uint16_t len);

/**
 * @brief Queue data received from TCP to be sent to BLE
 *
 * @param data Data buffer
 * @param len Data length
 * @return 0 on success, negative errno on failure
 */
int bridge_queue_tcp_to_ble(const uint8_t *data, uint16_t len);

#endif /* BRIDGE_H */
