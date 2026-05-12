#
# Author: Benjamin Sergeant
# Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
#

cmake_minimum_required (VERSION 3.14)
project (ws)

# There's -Weverything too for clang
if (NOT WIN32)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
endif()

set (CMAKE_CXX_STANDARD 11)

option(USE_TLS "Add TLS support" ON)

include(FetchContent)

set(CLI11_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CLI11_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CLI11_BUILD_DOCS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(cli11
    GIT_REPOSITORY "https://github.com/CLIUtils/CLI11"
    GIT_TAG "v2.6.2"
    GIT_SHALLOW 1)
FetchContent_Declare(cpp_linenoise
    GIT_REPOSITORY "https://github.com/yhirose/cpp-linenoise"
    GIT_TAG "5d2183e931eff05bc0d419a5eb84040a1d0af2c1"
    GIT_SHALLOW 1)
FetchContent_MakeAvailable(cli11 cpp_linenoise)
# Mark linenoise includes as system headers to suppress third-party warnings
set_target_properties(linenoise PROPERTIES
    INTERFACE_SYSTEM_INCLUDE_DIRECTORIES
    "$<TARGET_PROPERTY:linenoise,INTERFACE_INCLUDE_DIRECTORIES>")

include_directories(ws .)
include_directories(ws ..)

add_executable(ws
  ws.cpp)

# library with the most dependencies come first
target_link_libraries(ws ixwebsocket)

target_link_libraries(ws spdlog)

target_link_libraries(ws msgpack11)

target_link_libraries(ws CLI11::CLI11)

target_link_libraries(ws linenoise)

install(TARGETS ws RUNTIME DESTINATION bin)
