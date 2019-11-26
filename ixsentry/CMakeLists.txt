#
# Author: Benjamin Sergeant
# Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
#

set (IXSENTRY_SOURCES
    ixsentry/IXSentryClient.cpp
)

set (IXSENTRY_HEADERS
    ixsentry/IXSentryClient.h
)

add_library(ixsentry STATIC
  ${IXSENTRY_SOURCES}
  ${IXSENTRY_HEADERS}
)

set(IXSENTRY_INCLUDE_DIRS
    .
    ..
    ../ixcore
    ../third_party)

target_include_directories( ixsentry PUBLIC ${IXSENTRY_INCLUDE_DIRS} )
