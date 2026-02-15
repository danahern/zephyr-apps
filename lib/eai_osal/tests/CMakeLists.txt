cmake_minimum_required(VERSION 3.20.0)

# eai_osal is auto-discovered via the workspace-level zephyr/module.yml
# (cmake: lib → lib/CMakeLists.txt → add_subdirectory(eai_osal)).
# No ZEPHYR_EXTRA_MODULES needed.

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(eai_osal_test)

target_sources(app PRIVATE src/main.c)
