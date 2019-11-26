#
# Author: Benjamin Sergeant
# Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
#

cmake_minimum_required (VERSION 3.4.1)
project (ws)

# There's -Weverything too for clang
if (NOT WIN32)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
endif()

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
# set(CMAKE_LD_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")

set (CMAKE_CXX_STANDARD 14)

option(USE_TLS "Add TLS support" ON)

include_directories(ws .)
include_directories(ws ..)
include_directories(ws ../third_party)
include_directories(ws ../third_party/statsd-client-cpp/src)
include_directories(ws ../third_party/spdlog/include)
include_directories(ws ../third_party/cpp-linenoise)

if (UNIX)
  set( STATSD_CLIENT_SOURCES ../third_party/statsd-client-cpp/src/statsd_client.cpp)
endif()

add_executable(ws 
  ../third_party/msgpack11/msgpack11.cpp
  ../third_party/jsoncpp/jsoncpp.cpp
  ${STATSD_CLIENT_SOURCES}

  ws_http_client.cpp
  ws_ping_pong.cpp
  ws_broadcast_server.cpp
  ws_echo_server.cpp
  ws_chat.cpp
  ws_connect.cpp
  ws_transfer.cpp
  ws_send.cpp
  ws_receive.cpp
  ws_redis_publish.cpp
  ws_redis_subscribe.cpp
  ws_redis_server.cpp
  ws_snake.cpp
  ws_cobra_subscribe.cpp
  ws_cobra_metrics_publish.cpp
  ws_cobra_publish.cpp
  ws_cobra_to_statsd.cpp
  ws_cobra_to_sentry.cpp
  ws_cobra_metrics_to_redis.cpp
  ws_httpd.cpp
  ws_autobahn.cpp
  ws_proxy_server.cpp
  ws_sentry_minidump_upload.cpp
  ws.cpp)

target_link_libraries(ws ixsnake)
target_link_libraries(ws ixcobra)
target_link_libraries(ws ixwebsocket)
target_link_libraries(ws ixcrypto)
target_link_libraries(ws ixcore)
target_link_libraries(ws ixsentry)

if(NOT APPLE AND NOT USE_MBED_TLS)
  find_package(OpenSSL REQUIRED)
  add_definitions(${OPENSSL_DEFINITIONS})
  message(STATUS "OpenSSL: " ${OPENSSL_VERSION})
  include_directories(${OPENSSL_INCLUDE_DIR})
  target_link_libraries(ws ${OPENSSL_LIBRARIES})
endif()

install(TARGETS ws RUNTIME DESTINATION bin)
