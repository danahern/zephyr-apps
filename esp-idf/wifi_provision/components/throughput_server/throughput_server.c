/*
 * TCP Throughput Server — uses BSD sockets (lwIP) + OSAL thread/time.
 * Same protocol as the Zephyr version.
 */

#include "esp_log.h"

#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include <eai_osal/eai_osal.h>
#include "throughput_server.h"

static const char *TAG = "throughput";

#define THROUGHPUT_PORT 4242
#define STACK_SIZE      8192
#define BUF_SIZE        1024

#define CMD_ECHO   0x01
#define CMD_SINK   0x02
#define CMD_SOURCE 0x03

static void handle_client(int client_fd)
{
	uint8_t buf[BUF_SIZE];
	int ret;

	/* Read 1-byte command */
	ret = recv(client_fd, buf, 1, 0);
	if (ret != 1) {
		ESP_LOGE(TAG, "Failed to read command");
		return;
	}

	uint8_t cmd = buf[0];
	uint32_t start = eai_osal_time_get_ms();
	uint32_t last_report = start;
	size_t interval_bytes = 0;
	size_t total_bytes = 0;

	ESP_LOGI(TAG, "Client mode: 0x%02x", cmd);

	while (1) {
		uint32_t now = eai_osal_time_get_ms();

		/* Per-second stats */
		if (now - last_report >= 1000) {
			uint32_t dt_ms = now - last_report;
			uint32_t kbps = (interval_bytes * 8) / dt_ms;

			ESP_LOGI(TAG, "Throughput: %lu Kbps (%zu bytes in %lu ms)",
				 (unsigned long)kbps, interval_bytes,
				 (unsigned long)dt_ms);
			last_report = now;
			interval_bytes = 0;
		}

		switch (cmd) {
		case CMD_ECHO:
			ret = recv(client_fd, buf, sizeof(buf), 0);
			if (ret <= 0) {
				goto done;
			}
			interval_bytes += ret;
			total_bytes += ret;
			ret = send(client_fd, buf, ret, 0);
			if (ret < 0) {
				goto done;
			}
			break;

		case CMD_SINK:
			ret = recv(client_fd, buf, sizeof(buf), 0);
			if (ret <= 0) {
				goto done;
			}
			interval_bytes += ret;
			total_bytes += ret;
			break;

		case CMD_SOURCE:
			memset(buf, 0xAA, sizeof(buf));
			ret = send(client_fd, buf, sizeof(buf), 0);
			if (ret < 0) {
				goto done;
			}
			interval_bytes += ret;
			total_bytes += ret;
			break;

		default:
			ESP_LOGE(TAG, "Unknown command: 0x%02x", cmd);
			goto done;
		}
	}

done:;
	uint32_t elapsed_ms = eai_osal_time_get_ms() - start;

	if (elapsed_ms > 0) {
		uint32_t avg_kbps = (total_bytes * 8) / elapsed_ms;

		ESP_LOGI(TAG, "Session done: %zu bytes in %lu ms (%lu Kbps avg)",
			 total_bytes, (unsigned long)elapsed_ms,
			 (unsigned long)avg_kbps);
	}
}

static void throughput_thread_entry(void *arg)
{
	int server_fd;
	struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_port = htons(THROUGHPUT_PORT),
		.sin_addr.s_addr = htonl(INADDR_ANY),
	};

	server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (server_fd < 0) {
		ESP_LOGE(TAG, "Socket create failed");
		return;
	}

	int opt = 1;
	setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		ESP_LOGE(TAG, "Bind failed");
		close(server_fd);
		return;
	}

	if (listen(server_fd, 1) < 0) {
		ESP_LOGE(TAG, "Listen failed");
		close(server_fd);
		return;
	}

	ESP_LOGI(TAG, "Throughput server listening on port %d", THROUGHPUT_PORT);

	while (1) {
		struct sockaddr_in client_addr;
		socklen_t client_len = sizeof(client_addr);

		int client_fd = accept(server_fd,
				       (struct sockaddr *)&client_addr,
				       &client_len);
		if (client_fd < 0) {
			ESP_LOGE(TAG, "Accept failed");
			continue;
		}

		ESP_LOGI(TAG, "Client connected");
		handle_client(client_fd);
		close(client_fd);
		ESP_LOGI(TAG, "Client disconnected");
	}
}

EAI_OSAL_THREAD_STACK_DEFINE(throughput_stack, STACK_SIZE);
static eai_osal_thread_t throughput_thread;

void throughput_server_start(void)
{
	eai_osal_thread_create(&throughput_thread, "throughput",
			       throughput_thread_entry, NULL,
			       throughput_stack,
			       EAI_OSAL_THREAD_STACK_SIZEOF(throughput_stack),
			       7);
	ESP_LOGI(TAG, "Throughput server thread started");
}
