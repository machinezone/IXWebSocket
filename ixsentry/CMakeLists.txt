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

#
# Using try_compile or other techniques to detect std::regex
# availability is hard, so resorting to an ugly compiler and compiler
# version check.
#
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "4.9.0")
  else()
    target_compile_definitions( ixsentry PUBLIC HAVE_STD_REGEX=1 )
  endif()
else()
  target_compile_definitions( ixsentry PUBLIC HAVE_STD_REGEX=1 )
endif()

find_package(JsonCpp)
if (NOT JSONCPP_FOUND)
  set(JSONCPP_INCLUDE_DIRS ../third_party/jsoncpp)
endif()

set(IXSENTRY_INCLUDE_DIRS
    .
    ..
    ../ixcore
    ${JSONCPP_INCLUDE_DIRS})

target_include_directories( ixsentry PUBLIC ${IXSENTRY_INCLUDE_DIRS} )
