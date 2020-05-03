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
    ixwebsocket/IXBench.cpp
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
    ixwebsocket/IXUdpSocket.cpp
    ixwebsocket/IXUrlParser.cpp
    ixwebsocket/IXUserAgent.cpp
    ixwebsocket/IXWebSocket.cpp
    ixwebsocket/IXWebSocketCloseConstants.cpp
    ixwebsocket/IXWebSocketHandshake.cpp
    ixwebsocket/IXWebSocketHttpHeaders.cpp
    ixwebsocket/IXWebSocketPerMessageDeflate.cpp
    ixwebsocket/IXWebSocketPerMessageDeflateCodec.cpp
    ixwebsocket/IXWebSocketPerMessageDeflateOptions.cpp
    ixwebsocket/IXWebSocketServer.cpp
    ixwebsocket/IXWebSocketTransport.cpp
)

set( IXWEBSOCKET_HEADERS
    ixwebsocket/IXBench.h
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
    ixwebsocket/IXUdpSocket.h
    ixwebsocket/IXUrlParser.h
    ixwebsocket/IXUtf8Validator.h
    ixwebsocket/IXUserAgent.h
    ixwebsocket/IXWebSocket.h
    ixwebsocket/IXWebSocketCloseConstants.h
    ixwebsocket/IXWebSocketCloseInfo.h
    ixwebsocket/IXWebSocketErrorInfo.h
    ixwebsocket/IXWebSocketHandshake.h
    ixwebsocket/IXWebSocketHandshakeKeyGen.h
    ixwebsocket/IXWebSocketHttpHeaders.h
    ixwebsocket/IXWebSocketInitResult.h
    ixwebsocket/IXWebSocketMessage.h
    ixwebsocket/IXWebSocketMessageType.h
    ixwebsocket/IXWebSocketOpenInfo.h
    ixwebsocket/IXWebSocketPerMessageDeflate.h
    ixwebsocket/IXWebSocketPerMessageDeflateCodec.h
    ixwebsocket/IXWebSocketPerMessageDeflateOptions.h
    ixwebsocket/IXWebSocketSendInfo.h
    ixwebsocket/IXWebSocketServer.h
    ixwebsocket/IXWebSocketTransport.h
    ixwebsocket/IXWebSocketVersion.h
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
endif()

option(USE_TLS "Enable TLS support" FALSE)

if (USE_TLS)
    # default to securetranport on Apple if nothing is configured
    if (APPLE)
      if (NOT USE_MBED_TLS AND NOT USE_OPEN_SSL) # unless we want something else
        set(USE_SECURE_TRANSPORT ON)
      endif()
    else() # default to OpenSSL on all other platforms
      if (NOT USE_MBED_TLS) # Unless mbedtls is requested
        set(USE_OPEN_SSL ON)
      endif()
    endif()

    if (USE_MBED_TLS)
        list( APPEND IXWEBSOCKET_HEADERS ixwebsocket/IXSocketMbedTLS.h)
        list( APPEND IXWEBSOCKET_SOURCES ixwebsocket/IXSocketMbedTLS.cpp)
    elseif (USE_SECURE_TRANSPORT)
        list( APPEND IXWEBSOCKET_HEADERS ixwebsocket/IXSocketAppleSSL.h)
        list( APPEND IXWEBSOCKET_SOURCES ixwebsocket/IXSocketAppleSSL.cpp)
    elseif (USE_OPEN_SSL)
        list( APPEND IXWEBSOCKET_HEADERS ixwebsocket/IXSocketOpenSSL.h)
        list( APPEND IXWEBSOCKET_SOURCES ixwebsocket/IXSocketOpenSSL.cpp)
    else()
        message(FATAL_ERROR "TLS Configuration error: unknown backend")
    endif()
endif()

add_library( ixwebsocket STATIC
    ${IXWEBSOCKET_SOURCES}
    ${IXWEBSOCKET_HEADERS}
)

add_library ( ixwebsocket::ixwebsocket ALIAS ixwebsocket )

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

if (USE_TLS)
    target_compile_definitions(ixwebsocket PUBLIC IXWEBSOCKET_USE_TLS)
    if (USE_MBED_TLS)
        target_compile_definitions(ixwebsocket PUBLIC IXWEBSOCKET_USE_MBED_TLS)
    elseif (USE_OPEN_SSL)
        target_compile_definitions(ixwebsocket PUBLIC IXWEBSOCKET_USE_OPEN_SSL)
    elseif (USE_SECURE_TRANSPORT)
        target_compile_definitions(ixwebsocket PUBLIC IXWEBSOCKET_USE_SECURE_TRANSPORT)
    else()
        message(FATAL_ERROR "TLS Configuration error: unknown backend")
    endif()
endif()

if (USE_TLS)
  if (USE_OPEN_SSL)
    message(STATUS "TLS configured to use openssl")

    # Help finding Homebrew's OpenSSL on macOS
    if (APPLE)
      set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} /usr/local/opt/openssl/lib)
      set(CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH} /usr/local/opt/openssl/include)
    endif()

    # This OPENSSL_FOUND check is to help find a cmake manually configured OpenSSL
    if (NOT OPENSSL_FOUND)
	    include(FindOpenSSL)
    endif()
    message(STATUS "OpenSSL: " ${OPENSSL_VERSION})

    target_link_libraries(ixwebsocket PUBLIC OpenSSL::SSL OpenSSL::Crypto)
  elseif (USE_MBED_TLS)
    message(STATUS "TLS configured to use mbedtls")

    find_package(MbedTLS REQUIRED)
    target_include_directories(ixwebsocket PUBLIC ${MBEDTLS_INCLUDE_DIRS})
    target_link_libraries(ixwebsocket PUBLIC ${MBEDTLS_LIBRARIES})
  elseif (USE_SECURE_TRANSPORT)
    message(STATUS "TLS configured to use secure transport")
    target_link_libraries(ixwebsocket PUBLIC "-framework foundation" "-framework security")
  endif()
endif()

# This ZLIB_FOUND check is to help find a cmake manually configured zlib
if (NOT ZLIB_FOUND)
  find_package(ZLIB)
endif()
if (ZLIB_FOUND)
  include_directories(${ZLIB_INCLUDE_DIRS})
  target_link_libraries(ixwebsocket PUBLIC ${ZLIB_LIBRARIES})
else()
  include_directories(third_party/zlib ${CMAKE_CURRENT_BINARY_DIR}/third_party/zlib)
  add_subdirectory(third_party/zlib EXCLUDE_FROM_ALL)
  target_link_libraries(ixwebsocket PRIVATE $<LINK_ONLY:zlibstatic>)
endif()

if (WIN32)
  target_link_libraries(ixwebsocket PUBLIC wsock32 ws2_32 shlwapi)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)

  if (USE_TLS)
    target_link_libraries(ixwebsocket PUBLIC Crypt32)
  endif()
endif()

if (UNIX)
  find_package(Threads)
  target_link_libraries(ixwebsocket PUBLIC ${CMAKE_THREAD_LIBS_INIT})
endif()


set( IXWEBSOCKET_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    # Build with Multiple Processes
    target_compile_options(ixwebsocket PRIVATE /MP)
endif()

target_include_directories(ixwebsocket PUBLIC $<BUILD_INTERFACE:${IXWEBSOCKET_INCLUDE_DIRS}> $<INSTALL_INTERFACE:include/ixwebsocket>)

set_target_properties(ixwebsocket PROPERTIES PUBLIC_HEADER "${IXWEBSOCKET_HEADERS}")

install(TARGETS ixwebsocket EXPORT ixwebsocket
	ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
	PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_PREFIX}/include/ixwebsocket/
)

install(EXPORT ixwebsocket NAMESPACE ixwebsocket:: DESTINATION lib/cmake/ixwebsocket)
export(EXPORT ixwebsocket NAMESPACE ixwebsocket:: FILE ixwebsocketConfig.cmake)

if (USE_WS OR USE_TEST)
  add_subdirectory(ixcore)
  add_subdirectory(ixcrypto)
  add_subdirectory(ixcobra)
  add_subdirectory(ixsnake)
  add_subdirectory(ixsentry)
  add_subdirectory(ixbots)

  add_subdirectory(third_party/spdlog spdlog)

  if (USE_WS)
      add_subdirectory(ws)
  endif()
  if (USE_TEST)
      add_subdirectory(test)
  endif()
endif()
