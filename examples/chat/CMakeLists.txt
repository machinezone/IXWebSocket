#
# cmd_websocket_chat.cpp
# Author: Benjamin Sergeant
# Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
#

cmake_minimum_required (VERSION 3.4.1)
project (cmd_websocket_chat)

set (CMAKE_CXX_STANDARD 11)

option(USE_TLS "Add TLS support" ON)

add_subdirectory(${PROJECT_SOURCE_DIR}/../.. ixwebsocket)

add_executable(cmd_websocket_chat cmd_websocket_chat.cpp)

if (APPLE AND USE_TLS)
    target_link_libraries(cmd_websocket_chat "-framework foundation" "-framework security")
endif()

if (WIN32)
    target_link_libraries(cmd_websocket_chat wsock32 ws2_32)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

target_link_libraries(cmd_websocket_chat ixwebsocket)
install(TARGETS cmd_websocket_chat DESTINATION bin)
