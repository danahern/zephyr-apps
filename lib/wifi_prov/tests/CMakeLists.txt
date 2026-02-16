cmake_minimum_required(VERSION 3.20.0)

# wifi_prov is already discovered via zephyr-apps/zephyr/module.yml
# No ZEPHYR_EXTRA_MODULES needed

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(wifi_prov_test)

target_sources(app PRIVATE
    src/main.c
    src/test_cred.c
    src/test_msg.c
    src/test_sm.c
)
