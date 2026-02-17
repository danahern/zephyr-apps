cmake_minimum_required(VERSION 3.13)
project(eai_log_tests C)

set(LOG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# Unity
add_library(unity unity/unity.c)
target_include_directories(unity PUBLIC unity)

# Test executable
add_executable(eai_log_tests main.c)
target_include_directories(eai_log_tests PRIVATE ${LOG_DIR}/include)
target_compile_definitions(eai_log_tests PRIVATE CONFIG_EAI_LOG_BACKEND_POSIX)
target_link_libraries(eai_log_tests unity)
