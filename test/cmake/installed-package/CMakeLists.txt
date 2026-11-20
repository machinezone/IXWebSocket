cmake_minimum_required(VERSION 3.14)
project(ixwebsocket_installed_consumer LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Do not find Threads here: the installed package must find its own dependencies.
find_package(ixwebsocket CONFIG REQUIRED)
add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE ixwebsocket::ixwebsocket)
