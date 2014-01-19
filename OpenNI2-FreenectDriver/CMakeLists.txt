######################################################################################
# OpenNI2-FreenectDriver
######################################################################################

file(GLOB SOURCES src/*.cpp)
add_library(FreenectDriver SHARED ${SOURCES})

set(CMAKE_CXX_FLAGS "-Wno-gnu-static-float-init -Wno-unused-function")

set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib/OpenNI2-FreenectDriver)
set_target_properties( FreenectDriver PROPERTIES
  VERSION ${PROJECT_VER}
  SOVERSION ${PROJECT_APIVER}
  OUTPUT_NAME FreenectDriver)
  
include_directories(extern/OpenNI-Linux-x64-2.2.0.33/Include)
include_directories(${PROJECT_SOURCE_DIR}/wrappers/cpp)

target_link_libraries(FreenectDriver freenectstatic ${MATH_LIB})

install (TARGETS FreenectDriver
  DESTINATION "${PROJECT_LIBRARY_INSTALL_DIR}/OpenNI2-FreenectDriver")
