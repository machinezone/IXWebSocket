#
# cmd_websocket_chat.cpp
# Author: Benjamin Sergeant
# Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
#

cmake_minimum_required(VERSION 3.4.1)
project(ixwebsocket C CXX)

set (CMAKE_CXX_STANDARD 11)
set (CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_EXTENSIONS OFF)

set( IXWEBSOCKET_SOURCES
    ixwebsocket/IXSocket.cpp
    ixwebsocket/IXWebSocket.cpp
    ixwebsocket/IXWebSocketTransport.cpp
)

set( IXWEBSOCKET_HEADERS
    ixwebsocket/IXSocket.h
    ixwebsocket/IXWebSocket.h
    ixwebsocket/IXWebSocketTransport.h
)

if (USE_TLS)
    add_definitions(-DIXWEBSOCKET_USE_TLS)

    if (APPLE)
        list( APPEND IXWEBSOCKET_HEADERS ixwebsocket/IXSocketAppleSSL.h)
        list( APPEND IXWEBSOCKET_SOURCES ixwebsocket/IXSocketAppleSSL.cpp)
    elseif (WIN32)
        list( APPEND IXWEBSOCKET_HEADERS ixwebsocket/IXSocketSChannel.h)
        list( APPEND IXWEBSOCKET_SOURCES ixwebsocket/IXSocketSChannel.cpp)
    else()
        list( APPEND IXWEBSOCKET_HEADERS ixwebsocket/IXSocketOpenSSL.h)
        list( APPEND IXWEBSOCKET_SOURCES ixwebsocket/IXSocketOpenSSL.cpp)
    endif()
endif()

add_library( ixwebsocket STATIC
    ${IXWEBSOCKET_SOURCES}
    ${IXWEBSOCKET_HEADERS}
)

set( IXWEBSOCKET_INCLUDE_DIRS
    .
    ../../shared/OpenSSL/include)
target_include_directories( ixwebsocket PUBLIC ${IXWEBSOCKET_INCLUDE_DIRS} )
