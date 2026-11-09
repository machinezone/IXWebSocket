#
# Author: Benjamin Sergeant
# Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
#

cmake_minimum_required (VERSION 3.14)
project (ws)

option(USE_TLS "Add TLS support" ON)

include_directories(ws .)
include_directories(ws ..)

add_executable(ws
  ws.cpp)

target_compile_options(ws PRIVATE ${IXWEBSOCKET_WARNING_FLAGS})

# CLI11 is vendored as a single header in third_party, and included as a system
# header so that its warnings do not show up in our builds
target_include_directories(ws SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../third_party)

# library with the most dependencies come first
target_link_libraries(ws ixwebsocket)

install(TARGETS ws RUNTIME DESTINATION bin)
