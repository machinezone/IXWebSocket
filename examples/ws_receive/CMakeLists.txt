#
# Author: Benjamin Sergeant
# Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
#

cmake_minimum_required (VERSION 3.4.1)
project (ws_receive)

# There's -Weverything too for clang
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -Wshorten-64-to-32")

set (CMAKE_CXX_STANDARD 14)

option(USE_TLS "Add TLS support" ON)

add_subdirectory(${PROJECT_SOURCE_DIR}/../.. ixwebsocket)

include_directories(ws_receive .)

add_executable(ws_receive 
  jsoncpp/jsoncpp.cpp
  ixcrypto/IXBase64.cpp
  ixcrypto/IXHash.cpp
  ws_receive.cpp)

if (APPLE AND USE_TLS)
    target_link_libraries(ws_receive "-framework foundation" "-framework security")
endif()

target_link_libraries(ws_receive ixwebsocket)
