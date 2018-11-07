#
# Author: Benjamin Sergeant
# Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
#

cmake_minimum_required (VERSION 3.4.1)
project (satori_publisher)

set (OPENSSL_PREFIX /usr/local/opt/openssl) # Homebrew openssl

set (CMAKE_CXX_STANDARD 11)

option(USE_TLS "Add TLS support" ON)

add_subdirectory(${PROJECT_SOURCE_DIR}/../.. ixwebsocket)

include_directories(satori_publisher ${OPENSSL_PREFIX}/include)

add_executable(satori_publisher 
  base64.cpp
  jsoncpp/jsoncpp.cpp
  IXHMac.cpp
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
