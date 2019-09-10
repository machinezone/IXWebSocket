#
# Author: Benjamin Sergeant
# Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
#

set (IXCOBRA_SOURCES
    ixcobra/IXCobraConnection.cpp
    ixcobra/IXCobraMetricsThreadedPublisher.cpp
    ixcobra/IXCobraMetricsPublisher.cpp
)

set (IXCOBRA_HEADERS
    ixcobra/IXCobraConnection.h
    ixcobra/IXCobraMetricsThreadedPublisher.h
    ixcobra/IXCobraMetricsPublisher.h
)

add_library(ixcobra STATIC
    ${IXCOBRA_SOURCES}
    ${IXCOBRA_HEADERS}
)

set(IXCOBRA_INCLUDE_DIRS
    .
    ..
    ../ixcore
    ../ixcrypto
    ../third_party)

target_include_directories( ixcobra PUBLIC ${IXCOBRA_INCLUDE_DIRS} )
