cmake_minimum_required(VERSION 3.14)

# Normal parent variables must take precedence over defaults in the dependency.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED OFF)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS OFF)
project(ixwebsocket_build_settings LANGUAGES C CXX)

set(USE_TLS OFF CACHE BOOL "")
set(USE_ZLIB OFF CACHE BOOL "")
set(USE_WS ON CACHE BOOL "")
set(USE_TEST ON CACHE BOOL "")
add_subdirectory(../../.. ixwebsocket)

foreach(target ixwebsocket ws IXHttpTest)
  get_target_property(standard ${target} CXX_STANDARD)
  get_target_property(required ${target} CXX_STANDARD_REQUIRED)
  get_target_property(extensions ${target} CXX_EXTENSIONS)
  if (NOT standard STREQUAL "17" OR required OR NOT extensions)
    message(FATAL_ERROR "${target} changed the parent settings: ${standard};${required};${extensions}")
  endif()
endforeach()

get_target_property(library_source ixwebsocket SOURCE_DIR)
get_directory_property(export_commands DIRECTORY ${library_source}
  DEFINITION CMAKE_EXPORT_COMPILE_COMMANDS)
if (export_commands)
  message(FATAL_ERROR "IXWebSocket enabled compile commands despite the parent setting")
endif()
