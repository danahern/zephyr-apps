/*
 * TCP Socket Header
 */

#ifndef TCP_SOCKET_H
#define TCP_SOCKET_H

#include <zephyr/kernel.h>
#include <stdbool.h>

/* Callback type for received TCP data */
typedef void (*tcp_rx_cb_t)(const uint8_t *data, uint16_t len);

/**
 * @brief Initialize TCP socket module
 *
 * @param rx_callback Callback function for received data
 * @return 0 on success, negative errno on failure
 */
int tcp_socket_init(tcp_rx_cb_t rx_callback);

/**
 * @brief Connect to TCP server
 *
 * @return 0 on success, negative errno on failure
 */
int tcp_socket_connect(void);

/**
 * @brief Disconnect from TCP server
 */
void tcp_socket_disconnect(void);

/**
 * @brief Send data over TCP socket
 *
 * @param data Data to send
 * @param len Length of data
 * @return Number of bytes sent on success, negative errno on failure
 */
int tcp_socket_send(const uint8_t *data, uint16_t len);

/**
 * @brief Check if TCP socket is connected
 *
 * @return true if connected, false otherwise
 */
bool tcp_socket_is_connected(void);

/**
 * @brief Start TCP receive thread
 *
 * Must be called after connect succeeds.
 */
void tcp_socket_start_rx(void);

#endif /* TCP_SOCKET_H */
