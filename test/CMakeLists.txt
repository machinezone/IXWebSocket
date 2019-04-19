#
# Author: Benjamin Sergeant
# Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
#
cmake_minimum_required (VERSION 3.4.1)
project (ixwebsocket_unittest)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/../third_party/sanitizers-cmake/cmake" ${CMAKE_MODULE_PATH})
find_package(Sanitizers)

set (CMAKE_CXX_STANDARD 14)

if (NOT WIN32)
  option(USE_TLS "Add TLS support" ON)
endif()

add_subdirectory(${PROJECT_SOURCE_DIR}/.. ixwebsocket)

include_directories(
  ${PROJECT_SOURCE_DIR}/Catch2/single_include
  ../third_party/msgpack11
)

# Shared sources
set (SOURCES 
  test_runner.cpp
  IXTest.cpp
  ../third_party/msgpack11/msgpack11.cpp

  IXDNSLookupTest.cpp
  IXSocketTest.cpp
  IXSocketConnectTest.cpp
)

# Some unittest don't work on windows yet
if (NOT WIN32)
  list(APPEND SOURCES 
    IXWebSocketServerTest.cpp
    IXWebSocketPingTest.cpp
    IXWebSocketPingTimeoutTest.cpp
    cmd_websocket_chat.cpp
    IXWebSocketTestConnectionDisconnection.cpp
  )
endif()

add_executable(ixwebsocket_unittest ${SOURCES})
add_sanitizers(ixwebsocket_unittest)

if (APPLE AND USE_TLS)
    target_link_libraries(ixwebsocket_unittest "-framework foundation" "-framework security")
endif()

target_link_libraries(ixwebsocket_unittest ixwebsocket)
install(TARGETS ixwebsocket_unittest DESTINATION bin)
