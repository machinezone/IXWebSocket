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
include_directories(ws ../third_party/spdlog/include)
include_directories(ws ../third_party/cpp-linenoise)

add_definitions(-DSPDLOG_COMPILED_LIB=1)

find_package(JsonCpp)
if (NOT JSONCPP_FOUND)
  include_directories(../third_party/jsoncpp)
  set(JSONCPP_SOURCES ../third_party/jsoncpp/jsoncpp.cpp)
endif()

add_executable(ws
  ../third_party/msgpack11/msgpack11.cpp
  ${JSONCPP_SOURCES}

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
  ws_cobra_metrics_publish.cpp
  ws_cobra_publish.cpp
  ws_cobra_metrics_to_redis.cpp
  ws_httpd.cpp
  ws_autobahn.cpp
  ws_proxy_server.cpp
  ws_sentry_minidump_upload.cpp
  ws_dns_lookup.cpp
  ws.cpp)

# library with the most dependencies come first
target_link_libraries(ws ixbots)
target_link_libraries(ws ixsnake)
target_link_libraries(ws ixcobra)
target_link_libraries(ws ixsentry)
target_link_libraries(ws ixwebsocket)
target_link_libraries(ws ixcrypto)
target_link_libraries(ws ixcore)

target_link_libraries(ws spdlog)

if (JSONCPP_FOUND)
  target_include_directories(ws PUBLIC ${JSONCPP_INCLUDE_DIRS})
  target_link_libraries(ws ${JSONCPP_LIBRARIES})
endif()

install(TARGETS ws RUNTIME DESTINATION bin)
