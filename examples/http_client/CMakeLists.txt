#
# Author: Benjamin Sergeant
# Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
#

cmake_minimum_required (VERSION 3.4.1)
project (http_client)

set (CMAKE_CXX_STANDARD 14)

option(USE_TLS "Add TLS support" ON)

add_subdirectory(${PROJECT_SOURCE_DIR}/../.. ixwebsocket)

add_executable(http_client http_client.cpp)

if (APPLE AND USE_TLS)
    target_link_libraries(http_client "-framework foundation" "-framework security")
endif()

target_link_libraries(http_client ixwebsocket)
install(TARGETS http_client DESTINATION bin)
