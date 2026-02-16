#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>

#include "throughput_server.h"

LOG_MODULE_REGISTER(throughput, LOG_LEVEL_INF);

#ifndef CONFIG_THROUGHPUT_PORT
#define CONFIG_THROUGHPUT_PORT 4242
#endif

#define STACK_SIZE 8192
#define THREAD_PRIORITY 7
#define BUF_SIZE 1024

#define CMD_ECHO   0x01
#define CMD_SINK   0x02
#define CMD_SOURCE 0x03

static void handle_client(int client_fd)
{
	uint8_t buf[BUF_SIZE];
	int ret;

	/* Read 1-byte command */
	ret = zsock_recv(client_fd, buf, 1, 0);
	if (ret != 1) {
		LOG_ERR("Failed to read command");
		return;
	}

	uint8_t cmd = buf[0];
	int64_t start = k_uptime_get();
	int64_t last_report = start;
	size_t interval_bytes = 0;
	size_t total_bytes = 0;

	LOG_INF("Client mode: 0x%02x", cmd);

	while (true) {
		int64_t now = k_uptime_get();

		/* Per-second stats */
		if (now - last_report >= 1000) {
			int64_t dt_ms = now - last_report;
			uint32_t kbps = (interval_bytes * 8) / (uint32_t)dt_ms;

			LOG_INF("Throughput: %u Kbps (%zu bytes in %lld ms)",
				kbps, interval_bytes, dt_ms);
			last_report = now;
			interval_bytes = 0;
		}

		switch (cmd) {
		case CMD_ECHO:
			ret = zsock_recv(client_fd, buf, sizeof(buf), 0);
			if (ret <= 0) {
				goto done;
			}
			interval_bytes += ret;
			total_bytes += ret;
			ret = zsock_send(client_fd, buf, ret, 0);
			if (ret < 0) {
				goto done;
			}
			break;

		case CMD_SINK:
			ret = zsock_recv(client_fd, buf, sizeof(buf), 0);
			if (ret <= 0) {
				goto done;
			}
			interval_bytes += ret;
			total_bytes += ret;
			break;

		case CMD_SOURCE:
			memset(buf, 0xAA, sizeof(buf));
			ret = zsock_send(client_fd, buf, sizeof(buf), 0);
			if (ret < 0) {
				goto done;
			}
			interval_bytes += ret;
			total_bytes += ret;
			break;

		default:
			LOG_ERR("Unknown command: 0x%02x", cmd);
			goto done;
		}
	}

done:;
	int64_t elapsed_ms = k_uptime_get() - start;

	if (elapsed_ms > 0) {
		uint32_t avg_kbps = (total_bytes * 8) / (uint32_t)elapsed_ms;

		LOG_INF("Session done: %zu bytes in %lld ms (%u Kbps avg)",
			total_bytes, elapsed_ms, avg_kbps);
	}
}

static void throughput_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	int server_fd;
	struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_port = htons(CONFIG_THROUGHPUT_PORT),
		.sin_addr.s_addr = htonl(INADDR_ANY),
	};

	server_fd = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (server_fd < 0) {
		LOG_ERR("Socket create failed: %d", errno);
		return;
	}

	int opt = 1;

	zsock_setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	if (zsock_bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		LOG_ERR("Bind failed: %d", errno);
		zsock_close(server_fd);
		return;
	}

	if (zsock_listen(server_fd, 1) < 0) {
		LOG_ERR("Listen failed: %d", errno);
		zsock_close(server_fd);
		return;
	}

	LOG_INF("Throughput server listening on port %d", CONFIG_THROUGHPUT_PORT);

	while (true) {
		struct sockaddr_in client_addr;
		socklen_t client_len = sizeof(client_addr);

		int client_fd = zsock_accept(server_fd,
					     (struct sockaddr *)&client_addr,
					     &client_len);
		if (client_fd < 0) {
			LOG_ERR("Accept failed: %d", errno);
			continue;
		}

		LOG_INF("Client connected");
		handle_client(client_fd);
		zsock_close(client_fd);
		LOG_INF("Client disconnected");
	}
}

K_THREAD_STACK_DEFINE(throughput_stack, STACK_SIZE);
static struct k_thread throughput_thread_data;

void throughput_server_start(void)
{
	k_thread_create(&throughput_thread_data, throughput_stack,
			STACK_SIZE, throughput_thread,
			NULL, NULL, NULL,
			THREAD_PRIORITY, 0, K_NO_WAIT);
	k_thread_name_set(&throughput_thread_data, "throughput");
	LOG_INF("Throughput server thread started");
}
