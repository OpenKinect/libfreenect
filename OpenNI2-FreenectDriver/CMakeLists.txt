######################################################################################
# OpenNI2-FreenectDriver
######################################################################################

cmake_minimum_required(VERSION 3.8.2)

file(GLOB HEADERS src/*.hpp src/*.h)
file(GLOB SOURCES src/*.cpp)
add_library(FreenectDriver SHARED ${HEADERS} ${SOURCES})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-function")

set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib/OpenNI2-FreenectDriver)
set_target_properties(FreenectDriver PROPERTIES
  VERSION ${PROJECT_VER}
  SOVERSION ${PROJECT_APIVER}
  OUTPUT_NAME FreenectDriver)

add_definitions(-DPROJECT_VER="${PROJECT_VER}")

include_directories(extern/OpenNI-Linux-x64-2.2.0.33/Include)
include_directories(${PROJECT_SOURCE_DIR}/src)
include_directories(${PROJECT_SOURCE_DIR}/wrappers/cpp)

target_compile_features(FreenectDriver PUBLIC cxx_std_11 cxx_constexpr)

target_link_libraries(FreenectDriver freenectstatic ${MATH_LIB})

install (TARGETS FreenectDriver
  DESTINATION "${PROJECT_LIBRARY_INSTALL_DIR}/OpenNI2-FreenectDriver")
