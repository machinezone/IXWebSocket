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

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
# set(CMAKE_LD_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")

set (CMAKE_CXX_STANDARD 14)

include(FetchContent)
FetchContent_Declare(spdlog
    GIT_REPOSITORY "https://github.com/gabime/spdlog"
    GIT_TAG "v1.8.0") 

FetchContent_MakeAvailable(spdlog)

option(USE_TLS "Add TLS support" ON)

include_directories(ws .)
include_directories(ws ..)
include_directories(ws ../third_party/cpp-linenoise)

find_package(JsonCpp)
if (NOT JSONCPP_FOUND)
  include_directories(../third_party/jsoncpp)
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

add_executable(ws
  ../third_party/msgpack11/msgpack11.cpp
  ../third_party/cpp-linenoise/linenoise.cpp
  ${JSONCPP_SOURCES}
  ws.cpp)

# library with the most dependencies come first
target_link_libraries(ws ixbots)
target_link_libraries(ws ixsnake)
target_link_libraries(ws ixcobra)
target_link_libraries(ws ixsentry)
target_link_libraries(ws ixredis)
target_link_libraries(ws ixwebsocket)
target_link_libraries(ws ixcrypto)
target_link_libraries(ws ixcore)

target_link_libraries(ws spdlog)
if (USE_PYTHON)
  target_link_libraries(ws ${Python_LIBRARIES})
endif()

if (JSONCPP_FOUND)
  target_include_directories(ws PUBLIC ${JSONCPP_INCLUDE_DIRS})
  target_link_libraries(ws ${JSONCPP_LIBRARIES})
endif()

install(TARGETS ws RUNTIME DESTINATION bin)
