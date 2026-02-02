# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ble_wifi_bridge)

target_sources(app PRIVATE
    src/main.c
    src/ble_nus.c
    src/wifi_manager.c
    src/tcp_socket.c
    src/bridge.c
)

target_include_directories(app PRIVATE src)
