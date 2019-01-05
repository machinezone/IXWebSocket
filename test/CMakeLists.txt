#
# Author: Benjamin Sergeant
# Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
#

cmake_minimum_required (VERSION 3.4.1)
project (ixwebsocket_unittest)

set (CMAKE_CXX_STANDARD 11)

if (NOT WIN32)
  option(USE_TLS "Add TLS support" ON)
endif()

add_subdirectory(${PROJECT_SOURCE_DIR}/.. ixwebsocket)

include_directories(
  ${PROJECT_SOURCE_DIR}/Catch2/single_include
)

add_executable(ixwebsocket_unittest 
  test_runner.cpp
  cmd_websocket_chat.cpp
  IXWebSocketServerTest.cpp
  IXSocketTest.cpp
  IXTest.cpp
  msgpack11.cpp
)

if (APPLE AND USE_TLS)
    target_link_libraries(ixwebsocket_unittest "-framework foundation" "-framework security")
endif()

target_link_libraries(ixwebsocket_unittest ixwebsocket)
install(TARGETS ixwebsocket_unittest DESTINATION bin)
