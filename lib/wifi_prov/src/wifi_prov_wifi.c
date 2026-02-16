#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/dhcpv4.h>
#include <string.h>

#include <wifi_prov/wifi_prov.h>

LOG_MODULE_DECLARE(wifi_prov, LOG_LEVEL_INF);

static struct net_mgmt_event_callback wifi_cb;
static struct net_mgmt_event_callback ipv4_cb;
static wifi_prov_wifi_state_cb_t state_callback;
static wifi_prov_scan_result_cb_t scan_result_callback;
static bool wifi_connected;
static bool ip_obtained;
static struct net_if *wifi_iface;
static uint8_t current_ip[4];

static void handle_wifi_scan_result(struct net_mgmt_event_callback *cb)
{
	const struct wifi_scan_result *entry =
		(const struct wifi_scan_result *)cb->info;

	if (!scan_result_callback) {
		return;
	}

	struct wifi_prov_scan_result result = {0};

	result.ssid_len = MIN(entry->ssid_length, WIFI_PROV_SSID_MAX_LEN);
	memcpy(result.ssid, entry->ssid, result.ssid_len);
	result.rssi = entry->rssi;
	result.channel = entry->channel;

	switch (entry->security) {
	case WIFI_SECURITY_TYPE_NONE:
		result.security = WIFI_PROV_SEC_NONE;
		break;
	case WIFI_SECURITY_TYPE_PSK:
		result.security = WIFI_PROV_SEC_WPA_PSK;
		break;
	case WIFI_SECURITY_TYPE_PSK_SHA256:
		result.security = WIFI_PROV_SEC_WPA2_PSK_SHA256;
		break;
	case WIFI_SECURITY_TYPE_SAE:
		result.security = WIFI_PROV_SEC_WPA3_SAE;
		break;
	default:
		result.security = WIFI_PROV_SEC_WPA2_PSK;
		break;
	}

	scan_result_callback(&result);
}

static void handle_wifi_scan_done(struct net_mgmt_event_callback *cb)
{
	const struct wifi_status *status = (const struct wifi_status *)cb->info;

	LOG_INF("WiFi scan done (status %d)", status->status);
	scan_result_callback = NULL;
}

static void handle_wifi_connect_result(struct net_mgmt_event_callback *cb)
{
	const struct wifi_status *status = (const struct wifi_status *)cb->info;

	if (status->status) {
		LOG_ERR("WiFi connection failed: %d", status->status);
		wifi_connected = false;
		if (state_callback) {
			state_callback(false);
		}
	} else {
		LOG_INF("WiFi connected");
		wifi_connected = true;
		net_dhcpv4_start(wifi_iface);
	}
}

static void handle_wifi_disconnect_result(struct net_mgmt_event_callback *cb)
{
	LOG_INF("WiFi disconnected");
	wifi_connected = false;
	ip_obtained = false;
	memset(current_ip, 0, sizeof(current_ip));

	if (state_callback) {
		state_callback(false);
	}
}

static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb,
				    uint64_t mgmt_event, struct net_if *iface)
{
	switch (mgmt_event) {
	case NET_EVENT_WIFI_SCAN_RESULT:
		handle_wifi_scan_result(cb);
		break;
	case NET_EVENT_WIFI_SCAN_DONE:
		handle_wifi_scan_done(cb);
		break;
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
		struct net_if_ipv4 *ipv4 = iface->config.ip.ipv4;

		if (ipv4 && ipv4->unicast[0].ipv4.is_used) {
			struct in_addr *addr = &ipv4->unicast[0].ipv4.address.in_addr;

			memcpy(current_ip, &addr->s_addr, 4);
			LOG_INF("IPv4 address: %u.%u.%u.%u",
				current_ip[0], current_ip[1],
				current_ip[2], current_ip[3]);
		}

		ip_obtained = true;

		if (state_callback) {
			state_callback(true);
		}
	}
}

int wifi_prov_wifi_init(wifi_prov_wifi_state_cb_t state_cb)
{
	state_callback = state_cb;

	wifi_iface = net_if_get_default();
	if (!wifi_iface) {
		LOG_ERR("No default network interface");
		return -ENODEV;
	}

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

int wifi_prov_wifi_scan(wifi_prov_scan_result_cb_t result_cb)
{
	if (!wifi_iface) {
		return -ENODEV;
	}

	scan_result_callback = result_cb;

	int ret = net_mgmt(NET_REQUEST_WIFI_SCAN, wifi_iface, NULL, 0);

	if (ret) {
		LOG_ERR("WiFi scan request failed: %d", ret);
		scan_result_callback = NULL;
		return ret;
	}

	LOG_INF("WiFi scan started");
	return 0;
}

int wifi_prov_wifi_connect(const struct wifi_prov_cred *cred)
{
	struct wifi_connect_req_params params = {0};

	if (!wifi_iface || !cred) {
		return -EINVAL;
	}

	params.ssid = cred->ssid;
	params.ssid_length = cred->ssid_len;
	params.psk = cred->psk;
	params.psk_length = cred->psk_len;
	params.channel = WIFI_CHANNEL_ANY;
	params.band = WIFI_FREQ_BAND_2_4_GHZ;

	switch (cred->security) {
	case WIFI_PROV_SEC_NONE:
		params.security = WIFI_SECURITY_TYPE_NONE;
		break;
	case WIFI_PROV_SEC_WPA_PSK:
	case WIFI_PROV_SEC_WPA2_PSK:
		params.security = WIFI_SECURITY_TYPE_PSK;
		break;
	case WIFI_PROV_SEC_WPA2_PSK_SHA256:
		params.security = WIFI_SECURITY_TYPE_PSK_SHA256;
		break;
	case WIFI_PROV_SEC_WPA3_SAE:
		params.security = WIFI_SECURITY_TYPE_SAE;
		break;
	default:
		params.security = WIFI_SECURITY_TYPE_PSK;
		break;
	}

	LOG_INF("Connecting to WiFi (SSID len=%u)", cred->ssid_len);

	int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, wifi_iface,
			   &params, sizeof(params));
	if (ret) {
		LOG_ERR("WiFi connect request failed: %d", ret);
		return ret;
	}

	return 0;
}

int wifi_prov_wifi_disconnect(void)
{
	if (!wifi_iface) {
		return -ENODEV;
	}

	return net_mgmt(NET_REQUEST_WIFI_DISCONNECT, wifi_iface, NULL, 0);
}

int wifi_prov_wifi_get_ip(uint8_t ip_addr[4])
{
	if (!ip_obtained) {
		return -ENOTCONN;
	}

	memcpy(ip_addr, current_ip, 4);
	return 0;
}

bool wifi_prov_wifi_is_connected(void)
{
	return wifi_connected && ip_obtained;
}
