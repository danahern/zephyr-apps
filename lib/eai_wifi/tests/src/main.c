/*
 * eai_wifi build-only compile verification.
 *
 * Verifies that the Zephyr backend compiles correctly with WiFi enabled.
 * Not a functional test — just ensures headers and symbols resolve.
 */

#include <eai_wifi/eai_wifi.h>
#include <stddef.h>

int main(void)
{
	eai_wifi_init();
	return 0;
}
