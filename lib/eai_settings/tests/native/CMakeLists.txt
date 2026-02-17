cmake_minimum_required(VERSION 3.13)
project(eai_settings_tests C)

set(SETTINGS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# Unity
add_library(unity unity/unity.c)
target_include_directories(unity PUBLIC unity)

# Test executable
add_executable(eai_settings_tests
    main.c
    ${SETTINGS_DIR}/src/posix/settings.c
)
target_include_directories(eai_settings_tests PRIVATE ${SETTINGS_DIR}/include)
target_compile_definitions(eai_settings_tests PRIVATE
    CONFIG_EAI_SETTINGS_BACKEND_POSIX
    EAI_SETTINGS_BASE_PATH="/tmp/eai_settings_test"
)
target_link_libraries(eai_settings_tests unity pthread)

# Optional sanitizers
option(ENABLE_SANITIZERS "Enable ASan + UBSan" OFF)
if(ENABLE_SANITIZERS)
    target_compile_options(eai_settings_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(eai_settings_tests PRIVATE -fsanitize=address,undefined)
endif()
