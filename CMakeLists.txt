cmake_minimum_required(VERSION 3.20)
project(libmavlink)
find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 17)

add_executable(libmavlink main.cpp SerialInterface.h)
target_link_libraries(libmavlink Threads::Threads)