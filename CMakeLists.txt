#
# Author: Benjamin Sergeant
# Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
#

cmake_minimum_required(VERSION 3.4.1)
project(ixwebsocket C CXX)

set (CMAKE_CXX_STANDARD 14)
set (CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_EXTENSIONS OFF)

# -Wshorten-64-to-32 does not work with clang
if (NOT WIN32)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
endif()

set( IXWEBSOCKET_SOURCES
    ixwebsocket/IXSocket.cpp
    ixwebsocket/IXSocketServer.cpp
    ixwebsocket/IXSocketConnect.cpp
    ixwebsocket/IXSocketFactory.cpp
    ixwebsocket/IXDNSLookup.cpp
    ixwebsocket/IXCancellationRequest.cpp
    ixwebsocket/IXWebSocket.cpp
    ixwebsocket/IXWebSocketServer.cpp
    ixwebsocket/IXWebSocketTransport.cpp
    ixwebsocket/IXWebSocketHandshake.cpp
    ixwebsocket/IXWebSocketPerMessageDeflate.cpp
    ixwebsocket/IXWebSocketPerMessageDeflateCodec.cpp
    ixwebsocket/IXWebSocketPerMessageDeflateOptions.cpp
    ixwebsocket/IXWebSocketHttpHeaders.cpp
    ixwebsocket/IXHttpClient.cpp
    ixwebsocket/IXUrlParser.cpp
    ixwebsocket/IXSelectInterrupt.cpp
    ixwebsocket/IXSelectInterruptPipe.cpp
    ixwebsocket/IXSelectInterruptFactory.cpp
)

set( IXWEBSOCKET_HEADERS
    ixwebsocket/IXSocket.h
    ixwebsocket/IXSocketServer.h
    ixwebsocket/IXSocketConnect.h
    ixwebsocket/IXSocketFactory.h
    ixwebsocket/IXSetThreadName.h
    ixwebsocket/IXDNSLookup.h
    ixwebsocket/IXCancellationRequest.h
    ixwebsocket/IXProgressCallback.h
    ixwebsocket/IXWebSocket.h
    ixwebsocket/IXWebSocketServer.h
    ixwebsocket/IXWebSocketTransport.h
    ixwebsocket/IXWebSocketHandshake.h
    ixwebsocket/IXWebSocketSendInfo.h
    ixwebsocket/IXWebSocketErrorInfo.h
    ixwebsocket/IXWebSocketPerMessageDeflate.h
    ixwebsocket/IXWebSocketPerMessageDeflateCodec.h
    ixwebsocket/IXWebSocketPerMessageDeflateOptions.h
    ixwebsocket/IXWebSocketHttpHeaders.h
    ixwebsocket/libwshandshake.hpp
    ixwebsocket/IXHttpClient.h
    ixwebsocket/IXUrlParser.h
    ixwebsocket/IXSelectInterrupt.h
    ixwebsocket/IXSelectInterruptPipe.h
    ixwebsocket/IXSelectInterruptFactory.h
)

# Platform specific code
if (APPLE)
    list( APPEND IXWEBSOCKET_SOURCES ixwebsocket/apple/IXSetThreadName_apple.cpp)
elseif (WIN32)
    list( APPEND IXWEBSOCKET_SOURCES ixwebsocket/windows/IXSetThreadName_windows.cpp)
else()
    list( APPEND IXWEBSOCKET_SOURCES ixwebsocket/linux/IXSetThreadName_linux.cpp)
    list( APPEND IXWEBSOCKET_SOURCES ixwebsocket/IXSelectInterruptEventFd.cpp)
    list( APPEND IXWEBSOCKET_HEADERS ixwebsocket/IXSelectInterruptEventFd.h)
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

# gcc/Linux needs -pthread
find_package(Threads)

if(UNIX AND NOT APPLE)
  find_package(OpenSSL REQUIRED)
  add_definitions(${OPENSSL_DEFINITIONS})
  message(STATUS "OpenSSL: " ${OPENSSL_VERSION})
  include_directories(${OPENSSL_INCLUDE_DIR})
endif()

if (WIN32)
  get_filename_component(libz_path
    ${PROJECT_SOURCE_DIR}/third_party/ZLIB-Windows/zlib-1.2.11_deploy_v140/release_dynamic/x64/lib/zlib.lib
    ABSOLUTE)
  add_library(libz STATIC IMPORTED)
  set_target_properties(libz PROPERTIES IMPORTED_LOCATION
    ${libz_path})

  include_directories(${PROJECT_SOURCE_DIR}/third_party/ZLIB-Windows/zlib-1.2.11_deploy_v140/include)

  target_link_libraries(ixwebsocket libz wsock32 ws2_32)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
  
else()
  target_link_libraries(ixwebsocket 
    z ${OPENSSL_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
endif()

set( IXWEBSOCKET_INCLUDE_DIRS
    .
    ../../shared/OpenSSL/include)
target_include_directories( ixwebsocket PUBLIC ${IXWEBSOCKET_INCLUDE_DIRS} )

add_subdirectory(ws)
