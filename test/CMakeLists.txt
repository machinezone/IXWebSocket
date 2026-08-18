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
  IXWebSocketServerTest
  IXWebSocketTestConnectionDisconnection
  IXUrlParserTest
  # IXHttpClientTest ## FIXME httpbin.org does not seem normal
  IXUnityBuildsTest
  IXHttpTest
  IXDNSLookupTest
  IXWebSocketSubProtocolTest
  # IXWebSocketBroadcastTest ## FIXME was depending on cobra / take a broadcast server from ws
  IXStrCaseCompareTest
  IXExponentialBackoffTest
  IXWebSocketCloseTest
  IXWebSocketHostTest
  IXWebSocketIPv6Test
  IXWebSocketSendTimeoutTest
)

# Some unittest don't work on windows yet
# Windows without TLS does not have hmac yet
if (UNIX)
  list(APPEND TEST_TARGET_NAMES
    # Fail on Windows in CI probably because the pathing is wrong and
    # some resource files cannot be found
    IXHttpServerTest
    IXWebSocketChatTest
  )
endif()

if (USE_ZLIB)
  list(APPEND TEST_TARGET_NAMES
    IXWebSocketPerMessageDeflateCompressorTest
  )
endif()

# Ping test fails intermittently, disabling them for now
# IXWebSocketPingTest.cpp
# IXWebSocketPingTimeoutTest.cpp

# IXWebSocketLeakTest.cpp # commented until we have a fix for #224 /
# that was was fixed but now the test does not compile

# Disable tests for now that are failing or not reliable

# Catch2 is vendored as an amalgamated source file in third_party, so that
# nothing needs to be downloaded at configure time. It is built as its own
# target, with unity builds disabled (the file is large enough as it is) and
# included as a system header so that its warnings do not show up in our build.
add_library(Catch2 STATIC ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/catch_amalgamated.cpp)
target_include_directories(Catch2 SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../third_party)
target_compile_features(Catch2 PUBLIC cxx_std_14)
# We supply our own main, in test_runner.cpp
target_compile_definitions(Catch2 PRIVATE CATCH_AMALGAMATED_CUSTOM_MAIN)
set_target_properties(Catch2 PROPERTIES UNITY_BUILD OFF)

add_library(ixwebsocket_test)
target_sources(ixwebsocket_test PRIVATE
    ${JSONCPP_SOURCES}
    test_runner.cpp
    IXTest.cpp
)
target_compile_definitions(ixwebsocket_test PRIVATE ${TEST_PROGRAMS_DEFINITIONS})

target_link_libraries(ixwebsocket_test Catch2)
target_link_libraries(ixwebsocket_test ixwebsocket)

foreach(TEST_TARGET_NAME ${TEST_TARGET_NAMES})
  add_executable(${TEST_TARGET_NAME}
    ${TEST_TARGET_NAME}.cpp
  )

  if (APPLE AND USE_TLS)
    target_link_libraries(${TEST_TARGET_NAME} "-framework foundation" "-framework security")
  endif()

  # library with the most dependencies come first
  target_link_libraries(${TEST_TARGET_NAME} ixwebsocket_test)
  target_link_libraries(${TEST_TARGET_NAME} ixwebsocket)

  target_link_libraries(${TEST_TARGET_NAME} Catch2)

  add_test(NAME ${TEST_TARGET_NAME}
           COMMAND ${TEST_TARGET_NAME}
           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

endforeach()
