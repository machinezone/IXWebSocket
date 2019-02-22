#
# Author: Benjamin Sergeant
# Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
#

cmake_minimum_required (VERSION 3.4.1)
project (cobra_publisher)

# There's -Weverything too for clang
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -Wshorten-64-to-32")

set (OPENSSL_PREFIX /usr/local/opt/openssl) # Homebrew openssl

set (CMAKE_CXX_STANDARD 14)

option(USE_TLS "Add TLS support" ON)

include_directories(cobra_publisher ${OPENSSL_PREFIX}/include)
include_directories(cobra_publisher .)

add_executable(cobra_publisher 
  jsoncpp/jsoncpp.cpp
  ixcrypto/IXHMac.cpp
  ixcrypto/IXBase64.cpp
  IXCobraConnection.cpp
  cobra_publisher.cpp)

if (APPLE AND USE_TLS)
    target_link_libraries(cobra_publisher "-framework foundation" "-framework security")
endif()

get_filename_component(crypto_lib_path ${OPENSSL_PREFIX}/lib/libcrypto.a ABSOLUTE)
add_library(lib_crypto STATIC IMPORTED)
set_target_properties(lib_crypto PROPERTIES IMPORTED_LOCATION ${crypto_lib_path})

link_directories(/usr/local/opt/openssl/lib)
target_link_libraries(cobra_publisher ixwebsocket lib_crypto)
install(TARGETS cobra_publisher DESTINATION bin)
