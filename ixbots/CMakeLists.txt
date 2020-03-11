#
# Author: Benjamin Sergeant
# Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
#

set (IXBOTS_SOURCES
    ixbots/IXCobraToSentryBot.cpp
    ixbots/IXCobraToStatsdBot.cpp
    ixbots/IXQueueManager.cpp
)

set (IXBOTS_HEADERS
    ixbots/IXCobraToSentryBot.h
    ixbots/IXCobraToStatsdBot.h
    ixbots/IXQueueManager.h
)

add_library(ixbots STATIC
    ${IXBOTS_SOURCES}
    ${IXBOTS_HEADERS}
)

find_package(JsonCpp)
if (NOT JSONCPP_FOUND)
  set(JSONCPP_INCLUDE_DIRS ../third_party/jsoncpp)
endif()

find_package(SpdLog)
if (NOT SPDLOG_FOUND)
  set(SPDLOG_INCLUDE_DIRS ../third_party/spdlog/include)
endif()

set(STATSD_CLIENT_INCLUDE_DIRS ../third_party/statsd-client-cpp/src)

set(IXBOTS_INCLUDE_DIRS
    .
    ..
    ../ixcore
    ../ixcobra
    ../ixsentry
    ${JSONCPP_INCLUDE_DIRS}
    ${SPDLOG_INCLUDE_DIRS}
    ${STATSD_CLIENT_INCLUDE_DIRS})

target_include_directories( ixbots PUBLIC ${IXBOTS_INCLUDE_DIRS} )
