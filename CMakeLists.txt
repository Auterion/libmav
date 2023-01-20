cmake_minimum_required(VERSION 3.20)
project(libmavlink)

set(CMAKE_CXX_STANDARD 17)

add_executable(libmavlink main.cpp tinyxml2/tinyxml2.cpp SerialInterface.h)
