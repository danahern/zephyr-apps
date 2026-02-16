#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <wifi_prov/wifi_prov.h>
#include "throughput_server.h"

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

int main(void)
{
	int ret;

	LOG_INF("WiFi Provisioning App");

	ret = wifi_prov_init();
	if (ret) {
		LOG_ERR("wifi_prov_init failed: %d", ret);
		return ret;
	}

	ret = wifi_prov_start();
	if (ret) {
		LOG_ERR("wifi_prov_start failed: %d", ret);
		return ret;
	}

	throughput_server_start();

	return 0;
}
