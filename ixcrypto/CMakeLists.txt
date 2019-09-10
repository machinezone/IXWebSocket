#
# Author: Benjamin Sergeant
# Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
#

set (IXCRYPTO_SOURCES
    ixcrypto/IXHMac.cpp
    ixcrypto/IXBase64.cpp
    ixcrypto/IXUUid.cpp
    ixcrypto/IXHash.cpp
)

set (IXCRYPTO_HEADERS
    ixcrypto/IXHMac.h
    ixcrypto/IXBase64.h
    ixcrypto/IXUUid.h
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
