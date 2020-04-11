# Find package structure taken from libcurl

include(FindPackageHandleStandardArgs)

find_path(JSONCPP_INCLUDE_DIRS json/json.h)
find_library(JSONCPP_LIBRARY jsoncpp)

find_package_handle_standard_args(JsonCpp
    FOUND_VAR
      JSONCPP_FOUND
    REQUIRED_VARS
      JSONCPP_LIBRARY
      JSONCPP_INCLUDE_DIRS
    FAIL_MESSAGE
      "Could NOT find jsoncpp"
)

set(JSONCPP_INCLUDE_DIRS ${JSONCPP_INCLUDE_DIRS})
set(JSONCPP_LIBRARIES ${JSONCPP_LIBRARY})
