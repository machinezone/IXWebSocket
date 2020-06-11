#
# Author: Benjamin Sergeant
# Copyright (c) 2020 Machine Zone, Inc. All rights reserved.
#

set (IXREDIS_SOURCES
    ixredis/IXRedisClient.cpp
    ixredis/IXRedisServer.cpp
)

set (IXREDIS_HEADERS
    ixredis/IXRedisClient.h
    ixredis/IXRedisServer.h
)

add_library(ixredis STATIC
    ${IXREDIS_SOURCES}
    ${IXREDIS_HEADERS}
)

set(IXREDIS_INCLUDE_DIRS
    .
    ..
    ../ixcore
    ../ixwebsocket)

target_include_directories( ixredis PUBLIC ${IXREDIS_INCLUDE_DIRS} )
