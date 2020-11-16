#
# Author: Benjamin Sergeant
# Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
#
cmake_minimum_required (VERSION 3.14)
project (ixwebsocket_unittest)

set (CMAKE_CXX_STANDARD 11)

option(USE_TLS "Add TLS support" ON)

# Shared sources
set (TEST_TARGET_NAMES
  IXSocketTest
  IXSocketConnectTest
  # IXWebSocketLeakTest.cpp # commented until we have a fix for #224
  IXWebSocketServerTest
  IXWebSocketTestConnectionDisconnection
  IXUrlParserTest
  IXHttpClientTest
  IXHttpServerTest
  IXUnityBuildsTest
  IXHttpTest
  IXDNSLookupTest
  IXWebSocketSubProtocolTest
  IXSentryClientTest
  IXWebSocketChatTest
  IXWebSocketBroadcastTest
  IXWebSocketPerMessageDeflateCompressorTest
  IXStreamSqlTest
)

# Some unittest don't work on windows yet
# Windows without TLS does not have hmac yet
if (UNIX)
  list(APPEND TEST_TARGET_NAMES
    IXWebSocketCloseTest
    IXCobraChatTest
    IXCobraMetricsPublisherTest
    IXCobraToSentryBotTest
    IXCobraToStatsdBotTest
    IXCobraToStdoutBotTest
  )
endif()

# Ping test fails intermittently, disabling them for now
# IXWebSocketPingTest.cpp
# IXWebSocketPingTimeoutTest.cpp

# Disable tests for now that are failing or not reliable

find_package(JsonCpp)
if (NOT JSONCPP_FOUND)
  set(JSONCPP_SOURCES ../third_party/jsoncpp/jsoncpp.cpp)
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

add_library(ixwebsocket_test)
target_sources(ixwebsocket_test PRIVATE 
    ${JSONCPP_SOURCES}
    test_runner.cpp
    IXTest.cpp
    ../third_party/msgpack11/msgpack11.cpp
)
target_compile_definitions(ixwebsocket_test PRIVATE ${TEST_PROGRAMS_DEFINITIONS})
target_include_directories(ixwebsocket_test PRIVATE
  ${PROJECT_SOURCE_DIR}/Catch2/single_include
  ../third_party
)
target_link_libraries(ixwebsocket_test ixsnake)
target_link_libraries(ixwebsocket_test ixcobra)
target_link_libraries(ixwebsocket_test ixwebsocket)
target_link_libraries(ixwebsocket_test ixcrypto)
target_link_libraries(ixwebsocket_test spdlog)

foreach(TEST_TARGET_NAME ${TEST_TARGET_NAMES})
  add_executable(${TEST_TARGET_NAME}
    ${TEST_TARGET_NAME}.cpp
  )

  target_include_directories(${TEST_TARGET_NAME} PRIVATE
    ${PROJECT_SOURCE_DIR}/Catch2/single_include
    ../third_party
    ../third_party/msgpack11
  )

  target_compile_definitions(${TEST_TARGET_NAME} PRIVATE SPDLOG_COMPILED_LIB=1)

  if (NOT JSONCPP_FOUND)
    target_include_directories(${TEST_TARGET_NAME} PRIVATE ../third_party/jsoncpp)
  endif()

  if (APPLE AND USE_TLS)
    target_link_libraries(${TEST_TARGET_NAME} "-framework foundation" "-framework security")
  endif()

  # library with the most dependencies come first
  target_link_libraries(${TEST_TARGET_NAME} ixwebsocket_test)
  target_link_libraries(${TEST_TARGET_NAME} ixbots)
  target_link_libraries(${TEST_TARGET_NAME} ixsnake)
  target_link_libraries(${TEST_TARGET_NAME} ixcobra)
  target_link_libraries(${TEST_TARGET_NAME} ixsentry)

  if (JSONCPP_FOUND)
    target_include_directories(${TEST_TARGET_NAME} PRIVATE ${JSONCPP_INCLUDE_DIRS})
    target_link_libraries(${TEST_TARGET_NAME} ${JSONCPP_LIBRARIES})
  endif()

  target_link_libraries(${TEST_TARGET_NAME} ixredis)
  target_link_libraries(${TEST_TARGET_NAME} ixwebsocket)
  target_link_libraries(${TEST_TARGET_NAME} ixcrypto)
  target_link_libraries(${TEST_TARGET_NAME} ixcore)

  target_link_libraries(${TEST_TARGET_NAME} spdlog)
  if (USE_PYTHON)
    target_link_libraries(${TEST_TARGET_NAME} ${Python_LIBRARIES})
  endif()

  add_test(NAME ${TEST_TARGET_NAME}
           COMMAND ${TEST_TARGET_NAME}
           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

endforeach()
