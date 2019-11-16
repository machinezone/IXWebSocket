#
# Author: Benjamin Sergeant
# Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
#

cmake_minimum_required(VERSION 3.4.1)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake;${CMAKE_MODULE_PATH}")

project(ixwebsocket C CXX)

set (CMAKE_CXX_STANDARD 14)
set (CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_EXTENSIONS OFF)

if (UNIX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
endif()

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wshorten-64-to-32")
endif()

set( IXWEBSOCKET_SOURCES
    ixwebsocket/IXCancellationRequest.cpp
    ixwebsocket/IXConnectionState.cpp
    ixwebsocket/IXDNSLookup.cpp
    ixwebsocket/IXExponentialBackoff.cpp
    ixwebsocket/IXHttp.cpp
    ixwebsocket/IXHttpClient.cpp
    ixwebsocket/IXHttpServer.cpp
    ixwebsocket/IXNetSystem.cpp
    ixwebsocket/IXSelectInterrupt.cpp
    ixwebsocket/IXSelectInterruptFactory.cpp
    ixwebsocket/IXSocket.cpp
    ixwebsocket/IXSocketConnect.cpp
    ixwebsocket/IXSocketFactory.cpp
    ixwebsocket/IXSocketServer.cpp
    ixwebsocket/IXSocketTLSOptions.cpp
    ixwebsocket/IXUrlParser.cpp
    ixwebsocket/IXUserAgent.cpp
    ixwebsocket/IXWebSocket.cpp
    ixwebsocket/IXWebSocketCloseConstants.cpp
    ixwebsocket/IXWebSocketHandshake.cpp
    ixwebsocket/IXWebSocketHttpHeaders.cpp
    ixwebsocket/IXWebSocketMessageQueue.cpp
    ixwebsocket/IXWebSocketPerMessageDeflate.cpp
    ixwebsocket/IXWebSocketPerMessageDeflateCodec.cpp
    ixwebsocket/IXWebSocketPerMessageDeflateOptions.cpp
    ixwebsocket/IXWebSocketServer.cpp
    ixwebsocket/IXWebSocketTransport.cpp
    ixwebsocket/LUrlParser.cpp
)

set( IXWEBSOCKET_HEADERS
    ixwebsocket/IXCancellationRequest.h
    ixwebsocket/IXConnectionState.h
    ixwebsocket/IXDNSLookup.h
    ixwebsocket/IXExponentialBackoff.h
    ixwebsocket/IXHttp.h
    ixwebsocket/IXHttpClient.h
    ixwebsocket/IXHttpServer.h
    ixwebsocket/IXNetSystem.h
    ixwebsocket/IXProgressCallback.h
    ixwebsocket/IXSelectInterrupt.h
    ixwebsocket/IXSelectInterruptFactory.h
    ixwebsocket/IXSetThreadName.h
    ixwebsocket/IXSocket.h
    ixwebsocket/IXSocketConnect.h
    ixwebsocket/IXSocketFactory.h
    ixwebsocket/IXSocketServer.h
    ixwebsocket/IXSocketTLSOptions.h
    ixwebsocket/IXUrlParser.h
    ixwebsocket/IXUtf8Validator.h
    ixwebsocket/IXUserAgent.h
    ixwebsocket/IXWebSocket.h
    ixwebsocket/IXWebSocketCloseConstants.h
    ixwebsocket/IXWebSocketCloseInfo.h
    ixwebsocket/IXWebSocketErrorInfo.h
    ixwebsocket/IXWebSocketHandshake.h
    ixwebsocket/IXWebSocketHttpHeaders.h
    ixwebsocket/IXWebSocketInitResult.h
    ixwebsocket/IXWebSocketMessage.h
    ixwebsocket/IXWebSocketMessageQueue.h
    ixwebsocket/IXWebSocketMessageType.h
    ixwebsocket/IXWebSocketOpenInfo.h
    ixwebsocket/IXWebSocketPerMessageDeflate.h
    ixwebsocket/IXWebSocketPerMessageDeflateCodec.h
    ixwebsocket/IXWebSocketPerMessageDeflateOptions.h
    ixwebsocket/IXWebSocketSendInfo.h
    ixwebsocket/IXWebSocketServer.h
    ixwebsocket/IXWebSocketTransport.h
    ixwebsocket/IXWebSocketVersion.h
    ixwebsocket/LUrlParser.h
    ixwebsocket/libwshandshake.hpp
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
elseif (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
    list( APPEND IXWEBSOCKET_SOURCES ixwebsocket/freebsd/IXSetThreadName_freebsd.cpp)
else()
    list( APPEND IXWEBSOCKET_SOURCES ixwebsocket/linux/IXSetThreadName_linux.cpp)
    list( APPEND IXWEBSOCKET_SOURCES ixwebsocket/IXSelectInterruptEventFd.cpp)
    list( APPEND IXWEBSOCKET_HEADERS ixwebsocket/IXSelectInterruptEventFd.h)
endif()

if (WIN32)
  set(USE_MBED_TLS TRUE)
endif()

if (USE_TLS)
    if (USE_MBED_TLS)
        list( APPEND IXWEBSOCKET_HEADERS ixwebsocket/IXSocketMbedTLS.h)
        list( APPEND IXWEBSOCKET_SOURCES ixwebsocket/IXSocketMbedTLS.cpp)
    elseif (APPLE AND NOT USE_OPEN_SSL)
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

if (USE_TLS)
    target_compile_definitions(ixwebsocket PUBLIC IXWEBSOCKET_USE_TLS)
    if (USE_MBED_TLS)
        target_compile_definitions(ixwebsocket PUBLIC IXWEBSOCKET_USE_MBED_TLS)
    elseif (USE_OPEN_SSL)
        target_compile_definitions(ixwebsocket PUBLIC IXWEBSOCKET_USE_OPEN_SSL)
    elseif (APPLE)
    elseif (WIN32)
    else()
        target_compile_definitions(ixwebsocket PUBLIC IXWEBSOCKET_USE_OPEN_SSL)
    endif()
endif()

if (APPLE AND USE_TLS AND NOT USE_MBED_TLS)
  target_link_libraries(ixwebsocket "-framework foundation" "-framework security")
endif()

if (WIN32)
  target_link_libraries(ixwebsocket wsock32 ws2_32)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

if (UNIX)
  find_package(Threads)
  target_link_libraries(ixwebsocket ${CMAKE_THREAD_LIBS_INIT})
endif()

if (USE_TLS AND USE_OPEN_SSL)

  # Help finding Homebrew's OpenSSL on macOS
  if (APPLE)
    set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} /usr/local/opt/openssl/lib)
    set(CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH} /usr/local/opt/openssl/include)
  endif()

  find_package(OpenSSL REQUIRED)
  add_definitions(${OPENSSL_DEFINITIONS})
  message(STATUS "OpenSSL: " ${OPENSSL_VERSION})
  include_directories(${OPENSSL_INCLUDE_DIR})
  target_link_libraries(ixwebsocket ${OPENSSL_LIBRARIES})
endif()

if (USE_TLS AND USE_MBED_TLS)
  if (USE_VENDORED_THIRD_PARTY)
    set (ENABLE_PROGRAMS OFF)
    add_subdirectory(third_party/mbedtls)
    include_directories(third_party/mbedtls/include)

    target_link_libraries(ixwebsocket mbedtls)
  else()
    find_package(MbedTLS REQUIRED)
    target_include_directories(ixwebsocket PUBLIC ${MBEDTLS_INCLUDE_DIRS})
    target_link_libraries(ixwebsocket ${MBEDTLS_LIBRARIES})
  endif()
endif()

find_package(ZLIB)
if (ZLIB_FOUND)
  include_directories(${ZLIB_INCLUDE_DIRS})
  target_link_libraries(ixwebsocket ${ZLIB_LIBRARIES})
else()
  add_subdirectory(third_party/zlib)
  include_directories(third_party/zlib ${CMAKE_CURRENT_BINARY_DIR}/third_party/zlib)
  target_link_libraries(ixwebsocket zlibstatic)
endif()

set( IXWEBSOCKET_INCLUDE_DIRS
    .
)

if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    # Build with Multiple Processes
    target_compile_options(ixwebsocket PRIVATE /MP)
endif()

target_include_directories(ixwebsocket PUBLIC ${IXWEBSOCKET_INCLUDE_DIRS})

set_target_properties(ixwebsocket PROPERTIES PUBLIC_HEADER "${IXWEBSOCKET_HEADERS}")

install(TARGETS ixwebsocket
        ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_PREFIX}/include/ixwebsocket/
)

if (USE_WS OR USE_TEST)
  add_subdirectory(ixcore)
  add_subdirectory(ixcrypto)
  add_subdirectory(ixcobra)
  add_subdirectory(ixsnake)
  add_subdirectory(ixsentry)

  add_subdirectory(third_party/spdlog spdlog)

  if (USE_WS)
      add_subdirectory(ws)
  endif()
  if (USE_TEST)
      add_subdirectory(test)
  endif()
endif()
