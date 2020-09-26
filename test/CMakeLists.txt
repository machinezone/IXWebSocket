#
# Author: Benjamin Sergeant
# Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
#
cmake_minimum_required (VERSION 3.14)
project (ixwebsocket_unittest)

set (CMAKE_CXX_STANDARD 14)

option(USE_TLS "Add TLS support" ON)

include_directories(
  ${PROJECT_SOURCE_DIR}/Catch2/single_include
  ../third_party
  ../third_party/msgpack11
  ../ws
)

# Shared sources
set (SOURCES
  test_runner.cpp
  IXTest.cpp
  ../third_party/msgpack11/msgpack11.cpp

  IXSocketTest.cpp
  IXSocketConnectTest.cpp
  # IXWebSocketLeakTest.cpp # commented until we have a fix for #224
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
  IXWebSocketPerMessageDeflateCompressorTest.cpp
  IXStreamSqlTest.cpp
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

if (APPLE AND USE_TLS)
  target_link_libraries(ixwebsocket_unittest "-framework foundation" "-framework security")
endif()

if (JSONCPP_FOUND)
  target_include_directories(ixwebsocket_unittest PUBLIC ${JSONCPP_INCLUDE_DIRS})
  target_link_libraries(ixwebsocket_unittest ${JSONCPP_LIBRARIES})
endif()

if (USE_PYTHON)
  find_package(Python COMPONENTS Development)
  if (NOT Python_FOUND)
    message(FATAL_ERROR "Python3 not found")
  endif()
  message("Python_FOUND:${Python_FOUND}")
  message("Python_VERSION:${Python_VERSION}")
  message("Python_Development_FOUND:${Python_Development_FOUND}")
  message("Python_LIBRARIES:${Python_LIBRARIES}")
endif()

# library with the most dependencies come first
target_link_libraries(ixwebsocket_unittest ixbots)
target_link_libraries(ixwebsocket_unittest ixsnake)
target_link_libraries(ixwebsocket_unittest ixcobra)
target_link_libraries(ixwebsocket_unittest ixsentry)
target_link_libraries(ixwebsocket_unittest ixredis)
target_link_libraries(ixwebsocket_unittest ixwebsocket)
target_link_libraries(ixwebsocket_unittest ixcrypto)
target_link_libraries(ixwebsocket_unittest ixcore)

target_link_libraries(ixwebsocket_unittest spdlog)
target_link_libraries(ixwebsocket_unittest jsoncpp_static)
if (USE_PYTHON)
  target_link_libraries(ixwebsocket_unittest ${Python_LIBRARIES})
endif()

install(TARGETS ixwebsocket_unittest DESTINATION bin)
