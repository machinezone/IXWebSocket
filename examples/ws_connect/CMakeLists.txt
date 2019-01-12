#
# Author: Benjamin Sergeant
# Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
#

cmake_minimum_required (VERSION 3.4.1)
project (ws_connect)

set (CMAKE_CXX_STANDARD 14)

option(USE_TLS "Add TLS support" ON)

add_subdirectory(${PROJECT_SOURCE_DIR}/../.. ixwebsocket)

add_executable(ws_connect ws_connect.cpp)

if (APPLE AND USE_TLS)
    target_link_libraries(ws_connect "-framework foundation" "-framework security")
endif()

target_link_libraries(ws_connect ixwebsocket)
install(TARGETS ws_connect DESTINATION bin)
