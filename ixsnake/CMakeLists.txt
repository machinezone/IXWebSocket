#
# Author: Benjamin Sergeant
# Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
#

set (IXSNAKE_SOURCES
    ixsnake/IXSnakeServer.cpp
    ixsnake/IXSnakeProtocol.cpp
    ixsnake/IXAppConfig.cpp
)

set (IXSNAKE_HEADERS
    ixsnake/IXSnakeServer.h
    ixsnake/IXSnakeProtocol.h
    ixsnake/IXAppConfig.h
)

add_library(ixsnake STATIC
    ${IXSNAKE_SOURCES}
    ${IXSNAKE_HEADERS}
)

set(IXSNAKE_INCLUDE_DIRS
    .
    ..
    ../ixcore
    ../ixcrypto
    ../ixwebsocket
    ../ixredis
    ../third_party)

target_include_directories( ixsnake PUBLIC ${IXSNAKE_INCLUDE_DIRS} )
