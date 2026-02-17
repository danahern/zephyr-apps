/*
 * eai_ble build-only compile verification.
 *
 * Verifies that the Zephyr backend compiles correctly with BT enabled.
 * Not a functional test — just ensures headers and symbols resolve.
 */

#include <eai_ble/eai_ble.h>
#include <stddef.h>

int main(void)
{
	eai_ble_init(NULL);
	return 0;
}
