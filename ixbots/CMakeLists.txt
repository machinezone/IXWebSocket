#
# Author: Benjamin Sergeant
# Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
#

set (IXBOTS_SOURCES
    ixbots/IXCobraBot.cpp
    ixbots/IXCobraToSentryBot.cpp
    ixbots/IXCobraToStatsdBot.cpp
    ixbots/IXCobraToStdoutBot.cpp
    ixbots/IXCobraMetricsToStatsdBot.cpp
    ixbots/IXCobraMetricsToRedisBot.cpp
    ixbots/IXStatsdClient.cpp
)

set (IXBOTS_HEADERS
    ixbots/IXCobraBot.h
    ixbots/IXCobraBotConfig.h
    ixbots/IXCobraToSentryBot.h
    ixbots/IXCobraToStatsdBot.h
    ixbots/IXCobraToStdoutBot.h
    ixbots/IXCobraMetricsToStatsdBot.h
    ixbots/IXCobraMetricsToRedisBot.h
    ixbots/IXStatsdClient.h
)

add_library(ixbots STATIC
    ${IXBOTS_SOURCES}
    ${IXBOTS_HEADERS}
)

find_package(JsonCpp)
if (NOT JSONCPP_FOUND)
  set(JSONCPP_INCLUDE_DIRS ../third_party/jsoncpp)
endif()

set(IXBOTS_INCLUDE_DIRS
    .
    ..
    ../ixcore
    ../ixwebsocket
    ../ixcobra
    ../ixredis
    ../ixsentry
    ${JSONCPP_INCLUDE_DIRS}
    ${SPDLOG_INCLUDE_DIRS})

target_include_directories( ixbots PUBLIC ${IXBOTS_INCLUDE_DIRS} )
