/*
 * TCP Socket Implementation
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>

#include "tcp_socket.h"

LOG_MODULE_REGISTER(tcp_socket, LOG_LEVEL_INF);

#define TCP_RX_THREAD_STACK_SIZE 2048
#define TCP_RX_THREAD_PRIORITY 7
#define TCP_RX_BUFFER_SIZE CONFIG_BRIDGE_MSG_MAX_SIZE

static int sock_fd = -1;
static tcp_rx_cb_t rx_callback;
static bool connected;
static bool rx_thread_running;

K_THREAD_STACK_DEFINE(tcp_rx_stack, TCP_RX_THREAD_STACK_SIZE);
static struct k_thread tcp_rx_thread;

static void tcp_rx_thread_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	uint8_t buffer[TCP_RX_BUFFER_SIZE];

	LOG_INF("TCP RX thread started");

	while (connected && sock_fd >= 0) {
		int len = recv(sock_fd, buffer, sizeof(buffer), 0);

		if (len < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				continue;
			}
			LOG_ERR("TCP recv error: %d", errno);
			break;
		}

		if (len == 0) {
			LOG_INF("TCP connection closed by server");
			break;
		}

		LOG_INF("TCP RX: %d bytes", len);

		if (rx_callback) {
			rx_callback(buffer, len);
		}
	}

	LOG_INF("TCP RX thread exiting");
	connected = false;
	rx_thread_running = false;
}

int tcp_socket_init(tcp_rx_cb_t callback)
{
	rx_callback = callback;
	LOG_INF("TCP socket module initialized");
	return 0;
}

int tcp_socket_connect(void)
{
	struct sockaddr_in server_addr;
	int ret;

	if (sock_fd >= 0) {
		LOG_WRN("TCP socket already exists");
		return -EALREADY;
	}

	/* Create socket */
	sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock_fd < 0) {
		LOG_ERR("Failed to create socket: %d", errno);
		return -errno;
	}

	/* Set up server address */
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(CONFIG_BRIDGE_TCP_SERVER_PORT);

	ret = inet_pton(AF_INET, CONFIG_BRIDGE_TCP_SERVER_ADDR,
			&server_addr.sin_addr);
	if (ret != 1) {
		LOG_ERR("Invalid server address: %s", CONFIG_BRIDGE_TCP_SERVER_ADDR);
		close(sock_fd);
		sock_fd = -1;
		return -EINVAL;
	}

	LOG_INF("Connecting to %s:%d",
		CONFIG_BRIDGE_TCP_SERVER_ADDR, CONFIG_BRIDGE_TCP_SERVER_PORT);

	/* Connect to server */
	ret = connect(sock_fd, (struct sockaddr *)&server_addr,
		      sizeof(server_addr));
	if (ret < 0) {
		LOG_ERR("TCP connect failed: %d", errno);
		close(sock_fd);
		sock_fd = -1;
		return -errno;
	}

	connected = true;
	LOG_INF("TCP connected to %s:%d",
		CONFIG_BRIDGE_TCP_SERVER_ADDR, CONFIG_BRIDGE_TCP_SERVER_PORT);

	return 0;
}

void tcp_socket_disconnect(void)
{
	connected = false;

	if (sock_fd >= 0) {
		close(sock_fd);
		sock_fd = -1;
	}

	LOG_INF("TCP socket disconnected");
}

int tcp_socket_send(const uint8_t *data, uint16_t len)
{
	if (!connected || sock_fd < 0) {
		return -ENOTCONN;
	}

	int sent = send(sock_fd, data, len, 0);
	if (sent < 0) {
		LOG_ERR("TCP send failed: %d", errno);
		return -errno;
	}

	LOG_DBG("TCP TX: %d bytes", sent);
	return sent;
}

bool tcp_socket_is_connected(void)
{
	return connected && sock_fd >= 0;
}

void tcp_socket_start_rx(void)
{
	if (rx_thread_running) {
		LOG_WRN("TCP RX thread already running");
		return;
	}

	rx_thread_running = true;

	k_thread_create(&tcp_rx_thread, tcp_rx_stack,
			K_THREAD_STACK_SIZEOF(tcp_rx_stack),
			tcp_rx_thread_fn, NULL, NULL, NULL,
			TCP_RX_THREAD_PRIORITY, 0, K_NO_WAIT);

	k_thread_name_set(&tcp_rx_thread, "tcp_rx");
}
