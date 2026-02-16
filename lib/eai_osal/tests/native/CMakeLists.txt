cmake_minimum_required(VERSION 3.13)
project(osal_native_tests C)

# OSAL sources
set(OSAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)
file(GLOB OSAL_POSIX_SRCS ${OSAL_DIR}/src/posix/*.c)

# Unity
add_library(unity unity/unity.c)
target_include_directories(unity PUBLIC unity)

# Test executable
add_executable(osal_tests
    main.c
    ${OSAL_POSIX_SRCS}
)
target_include_directories(osal_tests PRIVATE ${OSAL_DIR}/include)
target_compile_definitions(osal_tests PRIVATE CONFIG_EAI_OSAL_BACKEND_POSIX)
target_link_libraries(osal_tests unity pthread)

# Optional sanitizers
option(ENABLE_SANITIZERS "Enable ASan + UBSan" OFF)
if(ENABLE_SANITIZERS)
    target_compile_options(osal_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(osal_tests PRIVATE -fsanitize=address,undefined)
endif()
