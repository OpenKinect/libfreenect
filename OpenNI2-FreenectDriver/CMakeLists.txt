######################################################################################
# OpenNI2-FreenectDriver
######################################################################################

file(GLOB HEADERS src/*.hpp src/*.h)
file(GLOB SOURCES src/*.cpp)
add_library(FreenectDriver SHARED ${HEADERS} ${SOURCES})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-function")

#if   (APPLE)
#  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -headerpad_max_install_names")
#endif(APPLE)

set(CMAKE_MACOSX_RPATH TRUE)

set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib/OpenNI2-FreenectDriver)
set_target_properties(
  FreenectDriver PROPERTIES
  VERSION ${PROJECT_VER}
  OUTPUT_NAME FreenectDriver
  # OSX install_name
  #BUILD_WITH_INSTALL_RPATH TRUE
  #INSTALL_NAME_DIR "@executable_path/../OpenNI2/Drivers/")
  
add_definitions(-DPROJECT_VER="${PROJECT_VER}")

include_directories(extern/OpenNI-Linux-x64-2.2.0.33/Include)
include_directories(${PROJECT_SOURCE_DIR}/wrappers/cpp)

target_link_libraries(FreenectDriver freenectstatic ${MATH_LIB})

install (TARGETS FreenectDriver
  DESTINATION "${PROJECT_LIBRARY_INSTALL_DIR}/OpenNI2/Drivers/")
