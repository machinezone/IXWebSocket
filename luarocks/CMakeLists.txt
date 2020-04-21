#
# Author: Benjamin Sergeant
# Copyright (c) 2020 Machine Zone, Inc. All rights reserved.
#

cmake_minimum_required (VERSION 3.4.1)
project (luarocks)

# There's -Weverything too for clang
if (NOT WIN32)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
endif()

set (CMAKE_CXX_STANDARD 14)

option(USE_TLS "Add TLS support" ON)

include_directories(luarocks .)

add_executable(luarocks main.cpp)

include(FindLua)
find_package(lua REQUIRED)
target_link_libraries(luarocks ${LUA_LIBRARIES})
target_include_directories(luarocks PRIVATE ${LUA_INCLUDE_DIR})

# library with the most dependencies come first
target_link_libraries(luarocks ixwebsocket)

install(TARGETS luarocks RUNTIME DESTINATION bin)
