/*
 * WiFi Manager Implementation
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/dhcpv4.h>

#include "wifi_manager.h"

LOG_MODULE_REGISTER(wifi_manager, LOG_LEVEL_INF);

static struct net_mgmt_event_callback wifi_cb;
static struct net_mgmt_event_callback ipv4_cb;
static wifi_state_cb_t state_callback;
static bool wifi_connected;
static bool ip_obtained;
static struct net_if *wifi_iface;

K_SEM_DEFINE(wifi_connect_sem, 0, 1);

static void handle_wifi_connect_result(struct net_mgmt_event_callback *cb)
{
	const struct wifi_status *status = (const struct wifi_status *)cb->info;

	if (status->status) {
		LOG_ERR("WiFi connection failed: %d", status->status);
		wifi_connected = false;
	} else {
		LOG_INF("WiFi connected");
		wifi_connected = true;
	}

	k_sem_give(&wifi_connect_sem);
}

static void handle_wifi_disconnect_result(struct net_mgmt_event_callback *cb)
{
	const struct wifi_status *status = (const struct wifi_status *)cb->info;

	if (status->status) {
		LOG_WRN("WiFi disconnect status: %d", status->status);
	}

	LOG_INF("WiFi disconnected");
	wifi_connected = false;
	ip_obtained = false;

	if (state_callback) {
		state_callback(false);
	}
}

static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb,
				    uint64_t mgmt_event, struct net_if *iface)
{
	switch (mgmt_event) {
	case NET_EVENT_WIFI_CONNECT_RESULT:
		handle_wifi_connect_result(cb);
		break;
	case NET_EVENT_WIFI_DISCONNECT_RESULT:
		handle_wifi_disconnect_result(cb);
		break;
	default:
		break;
	}
}

static void ipv4_mgmt_event_handler(struct net_mgmt_event_callback *cb,
				    uint64_t mgmt_event, struct net_if *iface)
{
	if (mgmt_event == NET_EVENT_IPV4_ADDR_ADD) {
		char addr_str[NET_IPV4_ADDR_LEN];
		struct net_if_ipv4 *ipv4 = iface->config.ip.ipv4;

		if (ipv4 && ipv4->unicast[0].ipv4.is_used) {
			struct in_addr *addr = &ipv4->unicast[0].ipv4.address.in_addr;
			net_addr_ntop(AF_INET, addr, addr_str, sizeof(addr_str));
			LOG_INF("IPv4 address: %s", addr_str);
		}

		ip_obtained = true;

		if (state_callback) {
			state_callback(true);
		}
	}
}

int wifi_manager_init(wifi_state_cb_t callback)
{
	state_callback = callback;

	/* Get WiFi interface */
	wifi_iface = net_if_get_default();
	if (!wifi_iface) {
		LOG_ERR("No default network interface");
		return -ENODEV;
	}

	/* Register WiFi management events */
	net_mgmt_init_event_callback(&wifi_cb, wifi_mgmt_event_handler,
				     NET_EVENT_WIFI_CONNECT_RESULT |
				     NET_EVENT_WIFI_DISCONNECT_RESULT);
	net_mgmt_add_event_callback(&wifi_cb);

	/* Register IPv4 events */
	net_mgmt_init_event_callback(&ipv4_cb, ipv4_mgmt_event_handler,
				     NET_EVENT_IPV4_ADDR_ADD);
	net_mgmt_add_event_callback(&ipv4_cb);

	LOG_INF("WiFi manager initialized");
	return 0;
}

int wifi_manager_connect(void)
{
	struct wifi_connect_req_params params = {0};

	params.ssid = (const uint8_t *)CONFIG_BRIDGE_WIFI_SSID;
	params.ssid_length = strlen(CONFIG_BRIDGE_WIFI_SSID);
	params.psk = (const uint8_t *)CONFIG_BRIDGE_WIFI_PSK;
	params.psk_length = strlen(CONFIG_BRIDGE_WIFI_PSK);
	params.channel = WIFI_CHANNEL_ANY;
	params.band = WIFI_FREQ_BAND_2_4_GHZ;

	if (params.psk_length > 0) {
		params.security = WIFI_SECURITY_TYPE_PSK;
	} else {
		params.security = WIFI_SECURITY_TYPE_NONE;
	}

	LOG_INF("Connecting to WiFi SSID: %s", CONFIG_BRIDGE_WIFI_SSID);

	int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, wifi_iface,
			   &params, sizeof(params));
	if (ret) {
		LOG_ERR("WiFi connect request failed: %d", ret);
		return ret;
	}

	/* Wait for connection result */
	ret = k_sem_take(&wifi_connect_sem, K_SECONDS(30));
	if (ret) {
		LOG_ERR("WiFi connection timeout");
		return -ETIMEDOUT;
	}

	if (!wifi_connected) {
		return -ECONNREFUSED;
	}

	/* Start DHCP if not already configured */
	net_dhcpv4_start(wifi_iface);

	return 0;
}

int wifi_manager_disconnect(void)
{
	int ret = net_mgmt(NET_REQUEST_WIFI_DISCONNECT, wifi_iface, NULL, 0);
	if (ret) {
		LOG_ERR("WiFi disconnect request failed: %d", ret);
		return ret;
	}

	return 0;
}

bool wifi_manager_is_connected(void)
{
	return wifi_connected && ip_obtained;
}
