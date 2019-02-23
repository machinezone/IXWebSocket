#
# Author: Benjamin Sergeant
# Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
#

cmake_minimum_required (VERSION 3.4.1)
project (ws)

# There's -Weverything too for clang
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")

set (CMAKE_CXX_STANDARD 14)

option(USE_TLS "Add TLS support" ON)

include_directories(ws .)
include_directories(ws ..)
include_directories(ws ../third_party)

add_executable(ws 
  ../third_party/msgpack11/msgpack11.cpp
  ixcrypto/IXBase64.cpp
  ixcrypto/IXHash.cpp
  ixcrypto/IXUuid.cpp

  ws_ping_pong.cpp
  ws_broadcast_server.cpp
  ws_echo_server.cpp
  ws_chat.cpp
  ws_connect.cpp
  ws_transfer.cpp
  ws_send.cpp
  ws_receive.cpp
  ws.cpp)

if (APPLE AND USE_TLS)
    target_link_libraries(ws "-framework foundation" "-framework security")
endif()

target_link_libraries(ws ixwebsocket)
install(TARGETS ws RUNTIME DESTINATION bin)
