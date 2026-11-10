cmake_minimum_required(VERSION 3.4.1...3.17.2)
project(ixwebsocket_atomic_consumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The installed target also carries the library's existing thread dependency.
find_package(Threads REQUIRED)
find_package(ixwebsocket CONFIG REQUIRED)
add_executable(atomic_consumer main.cpp)
target_link_libraries(atomic_consumer PRIVATE ixwebsocket::ixwebsocket)
