#
# Author: Benjamin Sergeant
# Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
#

cmake_minimum_required(VERSION 3.4.1)
project(ixwebsocket C CXX)

set (CMAKE_CXX_STANDARD 11)
set (CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_EXTENSIONS OFF)

set( IXWEBSOCKET_SOURCES
    ixwebsocket/IXEventFd.cpp
    ixwebsocket/IXSocket.cpp
    ixwebsocket/IXSocketConnect.cpp
    ixwebsocket/IXDNSLookup.cpp
    ixwebsocket/IXWebSocket.cpp
    ixwebsocket/IXWebSocketTransport.cpp
    ixwebsocket/IXWebSocketPerMessageDeflate.cpp
    ixwebsocket/IXWebSocketPerMessageDeflateOptions.cpp
)

set( IXWEBSOCKET_HEADERS
    ixwebsocket/IXEventFd.h
    ixwebsocket/IXSocket.h
    ixwebsocket/IXSocketConnect.h
    ixwebsocket/IXSetThreadName.h
    ixwebsocket/IXDNSLookup.h
    ixwebsocket/IXCancellationRequest.h
    ixwebsocket/IXWebSocket.h
    ixwebsocket/IXWebSocketTransport.h
    ixwebsocket/IXWebSocketSendInfo.h
    ixwebsocket/IXWebSocketErrorInfo.h
    ixwebsocket/IXWebSocketPerMessageDeflate.h
    ixwebsocket/IXWebSocketPerMessageDeflateOptions.h
    ixwebsocket/IXWebSocketHttpHeaders.h
)

# Platform specific code
if (APPLE)
    list( APPEND IXWEBSOCKET_SOURCES ixwebsocket/apple/IXSetThreadName_apple.cpp)
else()
    list( APPEND IXWEBSOCKET_SOURCES ixwebsocket/linux/IXSetThreadName_linux.cpp)
endif()


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

target_link_libraries(ixwebsocket "z")

set( IXWEBSOCKET_INCLUDE_DIRS
    .
    ../../shared/OpenSSL/include)
target_include_directories( ixwebsocket PUBLIC ${IXWEBSOCKET_INCLUDE_DIRS} )
