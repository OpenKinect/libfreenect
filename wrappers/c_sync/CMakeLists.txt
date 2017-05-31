######################################################################################
# C Synchronous Interface
######################################################################################

cmake_minimum_required(VERSION 3.5)

find_package(Threads REQUIRED)
list(APPEND CompileFeatures "cxx_alias_templates" "cxx_nullptr")

add_library(freenect_sync        SHARED libfreenect_sync.cpp)
add_library(freenect_sync_static STATIC libfreenect_sync.cpp)
target_compile_features(freenect_sync        PRIVATE ${CompileFeatures})
target_compile_features(freenect_sync_static PRIVATE ${CompileFeatures})
set_target_properties (freenect_sync_static PROPERTIES OUTPUT_NAME freenect_sync)

set_target_properties (freenect_sync PROPERTIES
  VERSION ${PROJECT_VER}
  SOVERSION ${PROJECT_APIVER})

target_link_libraries (freenect_sync freenect ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries (freenect_sync_static freenect ${CMAKE_THREAD_LIBS_INIT})

install (TARGETS freenect_sync
  DESTINATION "${PROJECT_LIBRARY_INSTALL_DIR}")
install (TARGETS freenect_sync_static
  DESTINATION "${PROJECT_LIBRARY_INSTALL_DIR}")
install (FILES "libfreenect_sync.h"
  DESTINATION ${PROJECT_INCLUDE_INSTALL_DIR})
