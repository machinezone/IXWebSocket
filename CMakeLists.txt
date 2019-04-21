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

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wshorten-64-to-32")
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
    ixwebsocket/IXSelectInterruptFactory.cpp
    ixwebsocket/IXConnectionState.cpp
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
    ixwebsocket/IXSelectInterruptFactory.h
    ixwebsocket/IXConnectionState.h
)

if (UNIX)
    # Linux, Mac, iOS, Android
    list( APPEND IXWEBSOCKET_SOURCES ixwebsocket/IXSelectInterruptPipe.cpp )
    list( APPEND IXWEBSOCKET_SOURCES ixwebsocket/IXSelectInterruptPipe.h )
endif()

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

set(USE_OPEN_SSL FALSE)
if (USE_TLS)
    add_definitions(-DIXWEBSOCKET_USE_TLS)

    if (APPLE)
        list( APPEND IXWEBSOCKET_HEADERS ixwebsocket/IXSocketAppleSSL.h)
        list( APPEND IXWEBSOCKET_SOURCES ixwebsocket/IXSocketAppleSSL.cpp)
    elseif (WIN32)
        list( APPEND IXWEBSOCKET_HEADERS ixwebsocket/IXSocketSChannel.h)
        list( APPEND IXWEBSOCKET_SOURCES ixwebsocket/IXSocketSChannel.cpp)
    else()
        set(USE_OPEN_SSL TRUE)
        list( APPEND IXWEBSOCKET_HEADERS ixwebsocket/IXSocketOpenSSL.h)
        list( APPEND IXWEBSOCKET_SOURCES ixwebsocket/IXSocketOpenSSL.cpp)
    endif()
endif()

add_library( ixwebsocket STATIC
    ${IXWEBSOCKET_SOURCES}
    ${IXWEBSOCKET_HEADERS}
)

if (APPLE AND USE_TLS)
  target_link_libraries(ixwebsocket "-framework foundation" "-framework security")
endif()

if (USE_OPEN_SSL)
  find_package(OpenSSL REQUIRED)
  add_definitions(${OPENSSL_DEFINITIONS})
  message(STATUS "OpenSSL: " ${OPENSSL_VERSION})
  include_directories(${OPENSSL_INCLUDE_DIR})
  target_link_libraries(ixwebsocket ${OPENSSL_LIBRARIES})
endif()

if (WIN32)
  add_subdirectory(third_party/zlib)
  include_directories(third_party/zlib ${CMAKE_CURRENT_BINARY_DIR}/third_party/zlib)
  target_link_libraries(ixwebsocket zlibstatic wsock32 ws2_32)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)

else()
  # gcc/Linux needs -pthread
  find_package(Threads)

  target_link_libraries(ixwebsocket 
    z ${CMAKE_THREAD_LIBS_INIT})
endif()

set( IXWEBSOCKET_INCLUDE_DIRS
    .
)

target_include_directories( ixwebsocket PUBLIC ${IXWEBSOCKET_INCLUDE_DIRS} )

set_target_properties(ixwebsocket PROPERTIES PUBLIC_HEADER "${IXWEBSOCKET_HEADERS}")

install(TARGETS ixwebsocket
        ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_PREFIX}/include/ixwebsocket/
)

if (NOT WIN32)
    add_subdirectory(ws)
endif()
