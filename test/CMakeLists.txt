#
# Author: Benjamin Sergeant
# Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
#
cmake_minimum_required (VERSION 3.4.1)
project (ixwebsocket_unittest)

set (CMAKE_CXX_STANDARD 14)

if (MAC)
  set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/../third_party/sanitizers-cmake/cmake" ${CMAKE_MODULE_PATH})
  find_package(Sanitizers)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
  set(CMAKE_LD_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
  option(USE_TLS "Add TLS support" ON)
endif()

include_directories(
  ${PROJECT_SOURCE_DIR}/Catch2/single_include
  ../third_party
  ../third_party/msgpack11
  ../third_party/spdlog/include
  ../ws
)

add_definitions(-DSPDLOG_COMPILED_LIB=1)

find_package(JsonCpp)
if (NOT JSONCPP_FOUND)
  include_directories(../third_party/jsoncpp)
  set(JSONCPP_SOURCES ../third_party/jsoncpp/jsoncpp.cpp)
endif()

# Shared sources
set (SOURCES
  ${JSONCPP_SOURCES}

  test_runner.cpp
  IXTest.cpp
  IXGetFreePort.cpp
  ../third_party/msgpack11/msgpack11.cpp

  IXSocketTest.cpp
  IXSocketConnectTest.cpp
  IXWebSocketServerTest.cpp
  IXWebSocketTestConnectionDisconnection.cpp
  IXUrlParserTest.cpp
  IXWebSocketServerTest.cpp
  IXHttpClientTest.cpp
  IXHttpServerTest.cpp
  IXUnityBuildsTest.cpp
  IXHttpTest.cpp
  IXDNSLookupTest.cpp
  IXWebSocketSubProtocolTest.cpp
  IXSentryClientTest.cpp
  IXWebSocketChatTest.cpp
  IXWebSocketBroadcastTest.cpp
)

# Some unittest don't work on windows yet
# Windows without TLS does not have hmac yet
if (UNIX)
  list(APPEND SOURCES
    IXWebSocketCloseTest.cpp
    IXCobraChatTest.cpp
    IXCobraMetricsPublisherTest.cpp
    IXCobraToSentryBotTest.cpp
    IXCobraToStatsdBotTest.cpp
    IXCobraToStdoutBotTest.cpp
  )
endif()

# Ping test fails intermittently, disabling them for now
# IXWebSocketPingTest.cpp
# IXWebSocketPingTimeoutTest.cpp

# Disable tests for now that are failing or not reliable

add_executable(ixwebsocket_unittest ${SOURCES})

if (MAC)
  add_sanitizers(ixwebsocket_unittest)
endif()

if (JSONCPP_FOUND)
  target_include_directories(ixwebsocket_unittest PRIVATE ${JSONCPP_INCLUDE_DIRS})
  target_link_libraries(ixwebsocket_unittest PRIVATE ${JSONCPP_LIBRARIES})
endif()

# library with the most dependencies come first
target_link_libraries(ixwebsocket_unittest PRIVATE ixbots)
target_link_libraries(ixwebsocket_unittest PRIVATE ixsnake)
target_link_libraries(ixwebsocket_unittest PRIVATE ixcobra)
target_link_libraries(ixwebsocket_unittest PRIVATE ixsentry)
target_link_libraries(ixwebsocket_unittest PRIVATE ixwebsocket)
target_link_libraries(ixwebsocket_unittest PRIVATE ixcrypto)
target_link_libraries(ixwebsocket_unittest PRIVATE ixcore)

target_link_libraries(ixwebsocket_unittest PRIVATE spdlog)

install(TARGETS ixwebsocket_unittest DESTINATION bin)
