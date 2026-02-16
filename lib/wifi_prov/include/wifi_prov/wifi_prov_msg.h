#ifndef WIFI_PROV_MSG_H
#define WIFI_PROV_MSG_H

#include <wifi_prov/wifi_prov_types.h>
#include <stddef.h>

/**
 * @brief Encode a scan result into a buffer.
 *
 * Wire format: [ssid_len:1][ssid:N][rssi:1][security:1][channel:1]
 *
 * @param result Scan result to encode.
 * @param buf Output buffer.
 * @param buf_size Size of output buffer.
 * @return Bytes written on success, -ENOBUFS if buffer too small, -EINVAL on bad input.
 */
int wifi_prov_msg_encode_scan_result(const struct wifi_prov_scan_result *result,
				     uint8_t *buf, size_t buf_size);

/**
 * @brief Decode a scan result from a buffer.
 *
 * @param buf Input buffer.
 * @param len Length of input data.
 * @param result Decoded scan result.
 * @return 0 on success, -EINVAL on malformed input.
 */
int wifi_prov_msg_decode_scan_result(const uint8_t *buf, size_t len,
				     struct wifi_prov_scan_result *result);

/**
 * @brief Encode credentials into a buffer.
 *
 * Wire format: [ssid_len:1][ssid:N][psk_len:1][psk:N][security:1]
 *
 * @param cred Credentials to encode.
 * @param buf Output buffer.
 * @param buf_size Size of output buffer.
 * @return Bytes written on success, -ENOBUFS if buffer too small, -EINVAL on bad input.
 */
int wifi_prov_msg_encode_credentials(const struct wifi_prov_cred *cred,
				     uint8_t *buf, size_t buf_size);

/**
 * @brief Decode credentials from a buffer.
 *
 * @param buf Input buffer.
 * @param len Length of input data.
 * @param cred Decoded credentials.
 * @return 0 on success, -EINVAL on malformed input.
 */
int wifi_prov_msg_decode_credentials(const uint8_t *buf, size_t len,
				     struct wifi_prov_cred *cred);

/**
 * @brief Encode status into a buffer.
 *
 * Wire format: [state:1][ip_addr:4]
 *
 * @param state Current provisioning state.
 * @param ip_addr IPv4 address (4 bytes, 0.0.0.0 if not connected).
 * @param buf Output buffer.
 * @param buf_size Size of output buffer.
 * @return Bytes written (always 5) on success, -ENOBUFS if buffer too small.
 */
int wifi_prov_msg_encode_status(enum wifi_prov_state state,
				const uint8_t ip_addr[4],
				uint8_t *buf, size_t buf_size);

/**
 * @brief Decode status from a buffer.
 *
 * @param buf Input buffer.
 * @param len Length of input data.
 * @param state Decoded state.
 * @param ip_addr Decoded IPv4 address (4 bytes).
 * @return 0 on success, -EINVAL on malformed input.
 */
int wifi_prov_msg_decode_status(const uint8_t *buf, size_t len,
				enum wifi_prov_state *state,
				uint8_t ip_addr[4]);

#endif /* WIFI_PROV_MSG_H */
