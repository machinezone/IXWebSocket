#
# Author: Benjamin Sergeant
# Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
#

cmake_minimum_required (VERSION 3.4.1)
project (echo_server)

# There's -Weverything too for clang
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -Wshorten-64-to-32")

set (OPENSSL_PREFIX /usr/local/opt/openssl) # Homebrew openssl

set (CMAKE_CXX_STANDARD 14)

option(USE_TLS "Add TLS support" ON)

add_subdirectory(${PROJECT_SOURCE_DIR}/../.. ixwebsocket)

include_directories(echo_server .)

add_executable(echo_server 
  echo_server.cpp)

if (APPLE AND USE_TLS)
    target_link_libraries(echo_server "-framework foundation" "-framework security")
endif()

target_link_libraries(echo_server ixwebsocket)
install(TARGETS echo_server DESTINATION bin)
