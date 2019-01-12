#
# Author: Benjamin Sergeant
# Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
#

cmake_minimum_required (VERSION 3.4.1)
project (ping_pong)

set (CMAKE_CXX_STANDARD 14)

option(USE_TLS "Add TLS support" ON)

add_subdirectory(${PROJECT_SOURCE_DIR}/../.. ixwebsocket)

add_executable(ping_pong ping_pong.cpp)

if (APPLE AND USE_TLS)
    target_link_libraries(ping_pong "-framework foundation" "-framework security")
endif()

if (WIN32)
    target_link_libraries(ping_pong wsock32 ws2_32)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

target_link_libraries(ping_pong ixwebsocket)
install(TARGETS ping_pong DESTINATION bin)
