cmake_minimum_required(VERSION 3.13)
project(eai_wifi_tests C)

set(WIFI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# Unity
add_library(unity unity/unity.c)
target_include_directories(unity PUBLIC unity)

# Test executable
add_executable(eai_wifi_tests
    main.c
    ${WIFI_DIR}/src/posix/wifi.c
)
target_include_directories(eai_wifi_tests PRIVATE ${WIFI_DIR}/include)
target_compile_definitions(eai_wifi_tests PRIVATE
    CONFIG_EAI_WIFI_BACKEND_POSIX
    EAI_WIFI_TEST
)
target_link_libraries(eai_wifi_tests unity)

# Optional sanitizers
option(ENABLE_SANITIZERS "Enable ASan + UBSan" OFF)
if(ENABLE_SANITIZERS)
    target_compile_options(eai_wifi_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(eai_wifi_tests PRIVATE -fsanitize=address,undefined)
endif()
