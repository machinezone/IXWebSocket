#
# Author: Benjamin Sergeant
# Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
#
include(CheckCSourceCompiles)

check_c_source_compiles("#include <regex>
                        int main()
                        { 
                          const std::regex dsnRegex;
                          std::smatch group;
                          std::regex_match(std::string(), group, dsnRegex); 
                          return 0;
                        }"
                        HAVE_STD_REGEX)

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

if (HAVE_STD_REGEX)
  target_compile_definitions( ixsentry PUBLIC HAVE_STD_REGEX=1 )
endif()
