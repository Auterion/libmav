cmake_minimum_required(VERSION 3.20)
project(mav VERSION 0.1.0 DESCRIPTION "mavlink library with a runtime defined message set")

include(GNUInstallDirs)
find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 17)

add_library(mav INTERFACE)
target_include_directories(libmavlink PRIVATE .)
target_include_directories(libmavlink PRIVATE include)

target_link_libraries(libmavlink Threads::Threads)

add_subdirectory(tests)