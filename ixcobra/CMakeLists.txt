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
    ixcobra/IXCobraConfig.h
    ixcobra/IXCobraEventType.h
)

add_library(ixcobra STATIC
    ${IXCOBRA_SOURCES}
    ${IXCOBRA_HEADERS}
)

find_package(JsonCpp)
if (NOT JSONCPP_FOUND)
  set(JSONCPP_INCLUDE_DIRS ../third_party/jsoncpp)
endif()

set(IXCOBRA_INCLUDE_DIRS
    .
    ..
    ../ixcore
    ../ixcrypto
    ${JSONCPP_INCLUDE_DIRS})

target_include_directories( ixcobra PUBLIC ${IXCOBRA_INCLUDE_DIRS} )
