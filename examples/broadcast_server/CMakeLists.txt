#
# Author: Benjamin Sergeant
# Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
#

cmake_minimum_required (VERSION 3.4.1)
project (broadcast_server)

# There's -Weverything too for clang
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -Wshorten-64-to-32")

set (OPENSSL_PREFIX /usr/local/opt/openssl) # Homebrew openssl

set (CMAKE_CXX_STANDARD 14)

option(USE_TLS "Add TLS support" ON)

add_subdirectory(${PROJECT_SOURCE_DIR}/../.. ixwebsocket)

include_directories(broadcast_server .)

add_executable(broadcast_server 
  broadcast_server.cpp)

if (APPLE AND USE_TLS)
    target_link_libraries(broadcast_server "-framework foundation" "-framework security")
endif()

target_link_libraries(broadcast_server ixwebsocket)
install(TARGETS broadcast_server DESTINATION bin)
