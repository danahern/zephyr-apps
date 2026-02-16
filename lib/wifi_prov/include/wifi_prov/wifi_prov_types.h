#ifndef WIFI_PROV_TYPES_H
#define WIFI_PROV_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#define WIFI_PROV_SSID_MAX_LEN  32
#define WIFI_PROV_PSK_MAX_LEN   64

/** WiFi security types */
enum wifi_prov_security {
	WIFI_PROV_SEC_NONE = 0,
	WIFI_PROV_SEC_WPA_PSK = 1,
	WIFI_PROV_SEC_WPA2_PSK = 2,
	WIFI_PROV_SEC_WPA2_PSK_SHA256 = 3,
	WIFI_PROV_SEC_WPA3_SAE = 4,
};

/** Provisioning state machine states */
enum wifi_prov_state {
	WIFI_PROV_STATE_IDLE = 0,
	WIFI_PROV_STATE_SCANNING = 1,
	WIFI_PROV_STATE_SCAN_COMPLETE = 2,
	WIFI_PROV_STATE_PROVISIONING = 3,
	WIFI_PROV_STATE_CONNECTING = 4,
	WIFI_PROV_STATE_CONNECTED = 5,
};

/** State machine events */
enum wifi_prov_event {
	WIFI_PROV_EVT_SCAN_TRIGGER,
	WIFI_PROV_EVT_SCAN_DONE,
	WIFI_PROV_EVT_CREDENTIALS_RX,
	WIFI_PROV_EVT_WIFI_CONNECTING,
	WIFI_PROV_EVT_WIFI_CONNECTED,
	WIFI_PROV_EVT_WIFI_FAILED,
	WIFI_PROV_EVT_WIFI_DISCONNECTED,
	WIFI_PROV_EVT_FACTORY_RESET,
};

/** Stored WiFi credentials */
struct wifi_prov_cred {
	uint8_t ssid[WIFI_PROV_SSID_MAX_LEN];
	uint8_t ssid_len;
	uint8_t psk[WIFI_PROV_PSK_MAX_LEN];
	uint8_t psk_len;
	uint8_t security;
};

/** WiFi AP scan result */
struct wifi_prov_scan_result {
	uint8_t ssid[WIFI_PROV_SSID_MAX_LEN];
	uint8_t ssid_len;
	int8_t rssi;
	uint8_t security;
	uint8_t channel;
};

#endif /* WIFI_PROV_TYPES_H */
