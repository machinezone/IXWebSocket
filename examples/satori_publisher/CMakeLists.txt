#
# Author: Benjamin Sergeant
# Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
#

cmake_minimum_required (VERSION 3.4.1)
project (satori_publisher)

# There's -Weverything too for clang
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -Wshorten-64-to-32")

set (OPENSSL_PREFIX /usr/local/opt/openssl) # Homebrew openssl

set (CMAKE_CXX_STANDARD 11)

option(USE_TLS "Add TLS support" ON)

add_subdirectory(${PROJECT_SOURCE_DIR}/../.. ixwebsocket)

include_directories(satori_publisher ${OPENSSL_PREFIX}/include)
include_directories(satori_publisher .)

add_executable(satori_publisher 
  jsoncpp/jsoncpp.cpp
  ixcrypto/IXHMac.cpp
  ixcrypto/IXBase64.cpp
  IXSatoriConnection.cpp
  satori_publisher.cpp)

if (APPLE AND USE_TLS)
    target_link_libraries(satori_publisher "-framework foundation" "-framework security")
endif()

get_filename_component(crypto_lib_path ${OPENSSL_PREFIX}/lib/libcrypto.a ABSOLUTE)
add_library(lib_crypto STATIC IMPORTED)
set_target_properties(lib_crypto PROPERTIES IMPORTED_LOCATION ${crypto_lib_path})

link_directories(/usr/local/opt/openssl/lib)
target_link_libraries(satori_publisher ixwebsocket lib_crypto)
install(TARGETS satori_publisher DESTINATION bin)
