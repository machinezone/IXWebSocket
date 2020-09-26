#
# Author: Benjamin Sergeant
# Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
#

set (IXBOTS_SOURCES
    ixbots/IXCobraBot.cpp
    ixbots/IXCobraToCobraBot.cpp
    ixbots/IXCobraToSentryBot.cpp
    ixbots/IXCobraToStatsdBot.cpp
    ixbots/IXCobraToStdoutBot.cpp
    ixbots/IXCobraMetricsToRedisBot.cpp
    ixbots/IXCobraToPythonBot.cpp
    ixbots/IXStatsdClient.cpp
)

set (IXBOTS_HEADERS
    ixbots/IXCobraBot.h
    ixbots/IXCobraBotConfig.h
    ixbots/IXCobraToCobraBot.h
    ixbots/IXCobraToSentryBot.h
    ixbots/IXCobraToStatsdBot.h
    ixbots/IXCobraToStdoutBot.h
    ixbots/IXCobraMetricsToRedisBot.h
    ixbots/IXCobraToPythonBot.h
    ixbots/IXStatsdClient.h
)

add_library(ixbots STATIC
    ${IXBOTS_SOURCES}
    ${IXBOTS_HEADERS}
)

if (USE_PYTHON)
  target_compile_definitions(ixbots PUBLIC IXBOTS_USE_PYTHON)
  find_package(Python COMPONENTS Development)
endif()

set(IXBOTS_INCLUDE_DIRS
    .
    ..
    ../ixcore
    ../ixwebsocket
    ../ixcobra
    ../ixredis
    ../ixsentry)

if (USE_PYTHON)
  set(IXBOTS_INCLUDE_DIRS ${IXBOTS_INCLUDE_DIRS} ${Python_INCLUDE_DIRS})
endif()

target_include_directories( ixbots PUBLIC ${IXBOTS_INCLUDE_DIRS} )
target_link_libraries( ixbots jsoncpp_static )
