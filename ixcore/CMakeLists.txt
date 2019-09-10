#
# Author: Benjamin Sergeant
# Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
#

set (IXCORE_SOURCES
    ixcore/utils/IXCoreLogger.cpp
)

set (IXCORE_HEADERS
    ixcore/utils/IXCoreLogger.h
)

add_library(ixcore STATIC
    ${IXCORE_SOURCES}
    ${IXCORE_HEADERS}
)

target_include_directories( ixcore PUBLIC . )
