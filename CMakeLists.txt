#
# Author: Benjamin Sergeant
# Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
#

cmake_minimum_required(VERSION 3.4.1...3.17.2)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake;${CMAKE_MODULE_PATH}")

project(ixwebsocket C CXX)

set (CMAKE_CXX_STANDARD 11)
set (CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_EXTENSIONS OFF)

option (BUILD_DEMO OFF)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

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
    ixwebsocket/IXGetFreePort.cpp
    ixwebsocket/IXGzipCodec.cpp
    ixwebsocket/IXHttp.cpp
    ixwebsocket/IXHttpClient.cpp
    ixwebsocket/IXHttpServer.cpp
    ixwebsocket/IXNetSystem.cpp
    ixwebsocket/IXSelectInterrupt.cpp
    ixwebsocket/IXSelectInterruptFactory.cpp
    ixwebsocket/IXSelectInterruptPipe.cpp
    ixwebsocket/IXSetThreadName.cpp
    ixwebsocket/IXSocket.cpp
    ixwebsocket/IXSocketConnect.cpp
    ixwebsocket/IXSocketFactory.cpp
    ixwebsocket/IXSocketServer.cpp
    ixwebsocket/IXSocketTLSOptions.cpp
    ixwebsocket/IXStrCaseCompare.cpp
    ixwebsocket/IXUdpSocket.cpp
    ixwebsocket/IXUrlParser.cpp
    ixwebsocket/IXUuid.cpp
    ixwebsocket/IXUserAgent.cpp
    ixwebsocket/IXWebSocket.cpp
    ixwebsocket/IXWebSocketCloseConstants.cpp
    ixwebsocket/IXWebSocketHandshake.cpp
    ixwebsocket/IXWebSocketHttpHeaders.cpp
    ixwebsocket/IXWebSocketPerMessageDeflate.cpp
    ixwebsocket/IXWebSocketPerMessageDeflateCodec.cpp
    ixwebsocket/IXWebSocketPerMessageDeflateOptions.cpp
    ixwebsocket/IXWebSocketProxyServer.cpp
    ixwebsocket/IXWebSocketServer.cpp
    ixwebsocket/IXWebSocketTransport.cpp
)

set( IXWEBSOCKET_HEADERS
    ixwebsocket/IXBench.h
    ixwebsocket/IXCancellationRequest.h
    ixwebsocket/IXConnectionState.h
    ixwebsocket/IXDNSLookup.h
    ixwebsocket/IXExponentialBackoff.h
    ixwebsocket/IXGetFreePort.h
    ixwebsocket/IXGzipCodec.h
    ixwebsocket/IXHttp.h
    ixwebsocket/IXHttpClient.h
    ixwebsocket/IXHttpServer.h
    ixwebsocket/IXNetSystem.h
    ixwebsocket/IXProgressCallback.h
    ixwebsocket/IXSelectInterrupt.h
    ixwebsocket/IXSelectInterruptFactory.h
    ixwebsocket/IXSelectInterruptPipe.h
    ixwebsocket/IXSetThreadName.h
    ixwebsocket/IXSocket.h
    ixwebsocket/IXSocketConnect.h
    ixwebsocket/IXSocketFactory.h
    ixwebsocket/IXSocketServer.h
    ixwebsocket/IXSocketTLSOptions.h
    ixwebsocket/IXStrCaseCompare.h
    ixwebsocket/IXUdpSocket.h
    ixwebsocket/IXUniquePtr.h
    ixwebsocket/IXUrlParser.h
    ixwebsocket/IXUuid.h
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
    ixwebsocket/IXWebSocketProxyServer.h
    ixwebsocket/IXWebSocketSendInfo.h
    ixwebsocket/IXWebSocketServer.h
    ixwebsocket/IXWebSocketTransport.h
    ixwebsocket/IXWebSocketVersion.h
)

option(BUILD_SHARED_LIBS "Build shared libraries (.dll/.so) instead of static ones (.lib/.a)" OFF)
option(USE_TLS "Enable TLS support" FALSE)

if (USE_TLS)
    # default to securetranport on Apple if nothing is configured
    if (APPLE)
      if (NOT USE_MBED_TLS AND NOT USE_OPEN_SSL) # unless we want something else
        set(USE_SECURE_TRANSPORT ON)
      endif()
    # default to mbedtls on windows if nothing is configured
    elseif (WIN32)
      if (NOT USE_OPEN_SSL) # unless we want something else
        set(USE_MBED_TLS ON)
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

add_library( ixwebsocket
    ${IXWEBSOCKET_SOURCES}
    ${IXWEBSOCKET_HEADERS}
)

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

      # This is for MacPort OpenSSL 1.0
      # set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} /opt/local/lib/openssl-1.0)
      # set(CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH} /opt/local/include/openssl-1.0)
    endif()

    # Use OPENSSL_ROOT_DIR CMake variable if you need to use your own openssl
    find_package(OpenSSL REQUIRED) 
    message(STATUS "OpenSSL: " ${OPENSSL_VERSION})

    add_definitions(${OPENSSL_DEFINITIONS})
    target_include_directories(ixwebsocket PUBLIC ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(ixwebsocket ${OPENSSL_LIBRARIES})
  elseif (USE_MBED_TLS)
    message(STATUS "TLS configured to use mbedtls")

    find_package(MbedTLS REQUIRED)
    target_include_directories(ixwebsocket PUBLIC ${MBEDTLS_INCLUDE_DIRS})
    target_link_libraries(ixwebsocket ${MBEDTLS_LIBRARIES})
  elseif (USE_SECURE_TRANSPORT)
    message(STATUS "TLS configured to use secure transport")
    target_link_libraries(ixwebsocket "-framework Foundation" "-framework Security")
  endif()
endif()

option(USE_ZLIB "Enable zlib support" TRUE)

if (USE_ZLIB)
  # Use ZLIB_ROOT CMake variable if you need to use your own zlib
  find_package(ZLIB REQUIRED)
  include_directories(${ZLIB_INCLUDE_DIRS})
  target_link_libraries(ixwebsocket ${ZLIB_LIBRARIES})

  target_compile_definitions(ixwebsocket PUBLIC IXWEBSOCKET_USE_ZLIB)
endif()

# brew install libdeflate
find_package(Deflate)
if (DEFLATE_FOUND)
  include_directories(${DEFLATE_INCLUDE_DIRS})
  target_link_libraries(ixwebsocket ${DEFLATE_LIBRARIES})
  target_compile_definitions(ixwebsocket PUBLIC IXWEBSOCKET_USE_DEFLATE)
endif()

if (WIN32)
  target_link_libraries(ixwebsocket wsock32 ws2_32 shlwapi)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)

  if (USE_TLS)
    target_link_libraries(ixwebsocket Crypt32)
  endif()
endif()

if (UNIX)
  find_package(Threads)
  target_link_libraries(ixwebsocket ${CMAKE_THREAD_LIBS_INIT})
endif()


set( IXWEBSOCKET_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    # Build with Multiple Processes
    target_compile_options(ixwebsocket PRIVATE /MP)
endif()

target_include_directories(ixwebsocket PUBLIC
  $<BUILD_INTERFACE:${IXWEBSOCKET_INCLUDE_DIRS}/>
  $<INSTALL_INTERFACE:include/ixwebsocket>
)

set_target_properties(ixwebsocket PROPERTIES PUBLIC_HEADER "${IXWEBSOCKET_HEADERS}")

install(TARGETS ixwebsocket
        EXPORT ixwebsocket
        ARCHIVE DESTINATION lib
        PUBLIC_HEADER DESTINATION include/ixwebsocket/
)

install(EXPORT ixwebsocket
        FILE ixwebsocket-config.cmake
        NAMESPACE ixwebsocket::
        DESTINATION lib/cmake/ixwebsocket)

if (USE_WS OR USE_TEST)
  include(FetchContent)
  FetchContent_Declare(spdlog
      GIT_REPOSITORY "https://github.com/gabime/spdlog"
      GIT_TAG "v1.8.0"
      GIT_SHALLOW 1) 

  FetchContent_MakeAvailable(spdlog)

  if (USE_WS)
    add_subdirectory(ws)
  endif()
  if (USE_TEST)
    enable_testing()
    add_subdirectory(test)
  endif()
endif()

if (BUILD_DEMO) 
  add_executable(demo main.cpp)
  target_link_libraries(demo ixwebsocket) 
endif()
