/*
 * eai_wifi Zephyr net_mgmt backend
 *
 * WiFi scan/connect/disconnect via Zephyr net_mgmt events.
 * DHCP started automatically on connect success.
 * EVT_CONNECTED fires only after IPv4 address is obtained.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/dhcpv4.h>
#include <string.h>

#include <eai_wifi/eai_wifi.h>

LOG_MODULE_REGISTER(eai_wifi, LOG_LEVEL_INF);

static struct net_mgmt_event_callback wifi_cb;
static struct net_mgmt_event_callback ipv4_cb;
static eai_wifi_event_cb_t event_cb;
static eai_wifi_scan_result_cb_t scan_result_cb;
static eai_wifi_scan_done_cb_t scan_done_cb;
static enum eai_wifi_state current_state;
static struct net_if *wifi_iface;
static uint8_t current_ip[4];

static void handle_scan_result(struct net_mgmt_event_callback *cb)
{
	const struct wifi_scan_result *entry =
		(const struct wifi_scan_result *)cb->info;

	if (!scan_result_cb) {
		return;
	}

	struct eai_wifi_scan_result result = {0};

	result.ssid_len = MIN(entry->ssid_length, EAI_WIFI_SSID_MAX_LEN);
	memcpy(result.ssid, entry->ssid, result.ssid_len);
	result.rssi = entry->rssi;
	result.channel = entry->channel;

	switch (entry->security) {
	case WIFI_SECURITY_TYPE_NONE:
		result.security = EAI_WIFI_SEC_OPEN;
		break;
	case WIFI_SECURITY_TYPE_PSK:
		result.security = EAI_WIFI_SEC_WPA2_PSK;
		break;
	case WIFI_SECURITY_TYPE_SAE:
		result.security = EAI_WIFI_SEC_WPA3_SAE;
		break;
	default:
		result.security = EAI_WIFI_SEC_WPA2_PSK;
		break;
	}

	scan_result_cb(&result);
}

static void handle_scan_done(struct net_mgmt_event_callback *cb)
{
	const struct wifi_status *status = (const struct wifi_status *)cb->info;
	eai_wifi_scan_done_cb_t cb_done = scan_done_cb;

	LOG_INF("WiFi scan done (status %d)", status->status);

	scan_result_cb = NULL;
	scan_done_cb = NULL;

	if (current_state == EAI_WIFI_STATE_SCANNING) {
		current_state = EAI_WIFI_STATE_DISCONNECTED;
	}

	if (cb_done) {
		cb_done(status->status);
	}
}

static void handle_connect_result(struct net_mgmt_event_callback *cb)
{
	const struct wifi_status *status = (const struct wifi_status *)cb->info;

	if (status->status) {
		LOG_ERR("WiFi connection failed: %d", status->status);
		current_state = EAI_WIFI_STATE_DISCONNECTED;
		if (event_cb) {
			event_cb(EAI_WIFI_EVT_CONNECT_FAILED);
		}
	} else {
		LOG_INF("WiFi connected, starting DHCP");
		net_dhcpv4_start(wifi_iface);
	}
}

static void handle_disconnect_result(struct net_mgmt_event_callback *cb)
{
	ARG_UNUSED(cb);

	LOG_INF("WiFi disconnected");
	current_state = EAI_WIFI_STATE_DISCONNECTED;
	memset(current_ip, 0, sizeof(current_ip));

	if (event_cb) {
		event_cb(EAI_WIFI_EVT_DISCONNECTED);
	}
}

static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb,
				     uint64_t mgmt_event, struct net_if *iface)
{
	switch (mgmt_event) {
	case NET_EVENT_WIFI_SCAN_RESULT:
		handle_scan_result(cb);
		break;
	case NET_EVENT_WIFI_SCAN_DONE:
		handle_scan_done(cb);
		break;
	case NET_EVENT_WIFI_CONNECT_RESULT:
		handle_connect_result(cb);
		break;
	case NET_EVENT_WIFI_DISCONNECT_RESULT:
		handle_disconnect_result(cb);
		break;
	default:
		break;
	}
}

static void ipv4_mgmt_event_handler(struct net_mgmt_event_callback *cb,
				     uint64_t mgmt_event, struct net_if *iface)
{
	if (mgmt_event != NET_EVENT_IPV4_ADDR_ADD) {
		return;
	}

	struct net_if_ipv4 *ipv4 = iface->config.ip.ipv4;

	if (ipv4 && ipv4->unicast[0].ipv4.is_used) {
		struct in_addr *addr = &ipv4->unicast[0].ipv4.address.in_addr;

		memcpy(current_ip, &addr->s_addr, 4);
		LOG_INF("IPv4 address: %u.%u.%u.%u",
			current_ip[0], current_ip[1],
			current_ip[2], current_ip[3]);
	}

	current_state = EAI_WIFI_STATE_CONNECTED;

	if (event_cb) {
		event_cb(EAI_WIFI_EVT_CONNECTED);
	}
}

int eai_wifi_init(void)
{
	wifi_iface = net_if_get_default();
	if (!wifi_iface) {
		LOG_ERR("No default network interface");
		return -ENODEV;
	}

	current_state = EAI_WIFI_STATE_DISCONNECTED;
	memset(current_ip, 0, sizeof(current_ip));

	net_mgmt_init_event_callback(&wifi_cb, wifi_mgmt_event_handler,
				     NET_EVENT_WIFI_SCAN_RESULT |
				     NET_EVENT_WIFI_SCAN_DONE |
				     NET_EVENT_WIFI_CONNECT_RESULT |
				     NET_EVENT_WIFI_DISCONNECT_RESULT);
	net_mgmt_add_event_callback(&wifi_cb);

	net_mgmt_init_event_callback(&ipv4_cb, ipv4_mgmt_event_handler,
				     NET_EVENT_IPV4_ADDR_ADD);
	net_mgmt_add_event_callback(&ipv4_cb);

	LOG_INF("WiFi manager initialized");
	return 0;
}

void eai_wifi_set_event_callback(eai_wifi_event_cb_t cb)
{
	event_cb = cb;
}

int eai_wifi_scan(eai_wifi_scan_result_cb_t on_result,
		  eai_wifi_scan_done_cb_t on_done)
{
	if (!wifi_iface) {
		return -ENODEV;
	}
	if (!on_result) {
		return -EINVAL;
	}

	scan_result_cb = on_result;
	scan_done_cb = on_done;
	current_state = EAI_WIFI_STATE_SCANNING;

	int ret = net_mgmt(NET_REQUEST_WIFI_SCAN, wifi_iface, NULL, 0);

	if (ret) {
		LOG_ERR("WiFi scan request failed: %d", ret);
		scan_result_cb = NULL;
		scan_done_cb = NULL;
		current_state = EAI_WIFI_STATE_DISCONNECTED;
		return ret;
	}

	LOG_INF("WiFi scan started");
	return 0;
}

int eai_wifi_connect(const uint8_t *ssid, uint8_t ssid_len,
		     const uint8_t *psk, uint8_t psk_len,
		     enum eai_wifi_security sec)
{
	if (!wifi_iface) {
		return -ENODEV;
	}
	if (!ssid || ssid_len == 0) {
		return -EINVAL;
	}

	struct wifi_connect_req_params params = {0};

	params.ssid = ssid;
	params.ssid_length = ssid_len;
	params.psk = psk;
	params.psk_length = psk_len;
	params.channel = WIFI_CHANNEL_ANY;
	params.band = WIFI_FREQ_BAND_2_4_GHZ;

	switch (sec) {
	case EAI_WIFI_SEC_OPEN:
		params.security = WIFI_SECURITY_TYPE_NONE;
		break;
	case EAI_WIFI_SEC_WPA_PSK:
	case EAI_WIFI_SEC_WPA2_PSK:
		params.security = WIFI_SECURITY_TYPE_PSK;
		break;
	case EAI_WIFI_SEC_WPA3_SAE:
		params.security = WIFI_SECURITY_TYPE_SAE;
		break;
	default:
		params.security = WIFI_SECURITY_TYPE_PSK;
		break;
	}

	current_state = EAI_WIFI_STATE_CONNECTING;

	LOG_INF("Connecting to WiFi (SSID len=%u)", ssid_len);

	int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, wifi_iface,
			   &params, sizeof(params));
	if (ret) {
		LOG_ERR("WiFi connect request failed: %d", ret);
		current_state = EAI_WIFI_STATE_DISCONNECTED;
		return ret;
	}

	return 0;
}

int eai_wifi_disconnect(void)
{
	if (!wifi_iface) {
		return -ENODEV;
	}

	return net_mgmt(NET_REQUEST_WIFI_DISCONNECT, wifi_iface, NULL, 0);
}

enum eai_wifi_state eai_wifi_get_state(void)
{
	return current_state;
}

int eai_wifi_get_ip(uint8_t ip[4])
{
	if (current_state != EAI_WIFI_STATE_CONNECTED) {
		return -ENOTCONN;
	}

	memcpy(ip, current_ip, 4);
	return 0;
}
