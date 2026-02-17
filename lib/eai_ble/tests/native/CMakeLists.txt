cmake_minimum_required(VERSION 3.13)
project(eai_ble_tests C)

set(BLE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# Unity
add_library(unity unity/unity.c)
target_include_directories(unity PUBLIC unity)

# Test executable
add_executable(eai_ble_tests
    main.c
    ${BLE_DIR}/src/posix/ble.c
)
target_include_directories(eai_ble_tests PRIVATE ${BLE_DIR}/include)
target_compile_definitions(eai_ble_tests PRIVATE
    CONFIG_EAI_BLE_BACKEND_POSIX
    EAI_BLE_TEST
)
target_link_libraries(eai_ble_tests unity)

# Optional sanitizers
option(ENABLE_SANITIZERS "Enable ASan + UBSan" OFF)
if(ENABLE_SANITIZERS)
    target_compile_options(eai_ble_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(eai_ble_tests PRIVATE -fsanitize=address,undefined)
endif()
