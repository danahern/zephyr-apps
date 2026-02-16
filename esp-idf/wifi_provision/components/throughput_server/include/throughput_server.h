#ifndef THROUGHPUT_SERVER_H
#define THROUGHPUT_SERVER_H

/**
 * @brief Start the TCP throughput server thread.
 *
 * Listens on port 4242.
 * Supports three modes:
 *   0x01 = echo: read and echo back
 *   0x02 = sink: read and discard
 *   0x03 = source: write continuously
 *
 * Runs in its own thread, call once after WiFi provisioning init.
 */
void throughput_server_start(void);

#endif /* THROUGHPUT_SERVER_H */
