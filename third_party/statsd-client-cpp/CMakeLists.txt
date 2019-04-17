cmake_minimum_required(VERSION 3.1)
project(helloCLion)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

include_directories(
    src
)

add_library(statsdcppclient STATIC src/statsd_client.cpp)
add_definitions("-fPIC")
target_link_libraries(statsdcppclient pthread)

add_executable(system_monitor demo/system_monitor.cpp)
target_link_libraries(system_monitor statsdcppclient)

add_executable(test_client demo/test_client.cpp)
target_link_libraries(test_client statsdcppclient)
