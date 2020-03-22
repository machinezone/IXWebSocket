#
# Author: Benjamin Sergeant
# Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
#
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../CMake;${CMAKE_MODULE_PATH}")

set (IXCRYPTO_SOURCES
    ixcrypto/IXHMac.cpp
    ixcrypto/IXBase64.cpp
    ixcrypto/IXUuid.cpp
    ixcrypto/IXHash.cpp
)

set (IXCRYPTO_HEADERS
    ixcrypto/IXHMac.h
    ixcrypto/IXBase64.h
    ixcrypto/IXUuid.h
    ixcrypto/IXHash.h
)

add_library(ixcrypto STATIC
    ${IXCRYPTO_SOURCES}
    ${IXCRYPTO_HEADERS}
)

set(IXCRYPTO_INCLUDE_DIRS
    .
    ../ixcore)

target_include_directories( ixcrypto PUBLIC ${IXCRYPTO_INCLUDE_DIRS} )

# hmac computation needs a crypto library

target_compile_definitions(ixcrypto PUBLIC IXCRYPTO_USE_TLS)
if (USE_MBED_TLS)
    find_package(MbedTLS REQUIRED)
    target_include_directories(ixcrypto PUBLIC ${MBEDTLS_INCLUDE_DIRS})
    target_link_libraries(ixcrypto ${MBEDTLS_LIBRARIES})
    target_compile_definitions(ixcrypto PUBLIC IXCRYPTO_USE_MBED_TLS)
elseif (APPLE)
elseif (WIN32)
else()
    find_package(OpenSSL REQUIRED)
    add_definitions(${OPENSSL_DEFINITIONS})
    message(STATUS "OpenSSL: " ${OPENSSL_VERSION})
    include_directories(${OPENSSL_INCLUDE_DIR})
    target_link_libraries(ixcrypto ${OPENSSL_LIBRARIES})
    target_compile_definitions(ixcrypto PUBLIC IXCRYPTO_USE_OPEN_SSL)
endif()
