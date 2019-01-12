#
# cmd_websocket_chat.cpp
# Author: Benjamin Sergeant
# Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
#

cmake_minimum_required (VERSION 3.4.1)
project (cmd_websocket_chat)

set (CMAKE_CXX_STANDARD 14)

option(USE_TLS "Add TLS support" ON)

add_subdirectory(${PROJECT_SOURCE_DIR}/../.. ixwebsocket)

add_executable(cmd_websocket_chat cmd_websocket_chat.cpp)

if (APPLE AND USE_TLS)
    target_link_libraries(cmd_websocket_chat "-framework foundation" "-framework security")
endif()

target_link_libraries(cmd_websocket_chat ixwebsocket)
install(TARGETS cmd_websocket_chat DESTINATION bin)
