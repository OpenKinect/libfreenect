PROJECT(libfreenect)

cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/Modules/")

set(CMAKE_C_FLAGS "-Wall -O3 -g")

add_subdirectory (lib)
add_subdirectory (examples)
