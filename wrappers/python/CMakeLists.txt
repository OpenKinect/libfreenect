######################################################################################
# Python extension builder
######################################################################################

cmake_minimum_required(VERSION 3.12.4)

set(Python2_EXACTVERSION CACHE STRING "exact version of python2")
set(Python3_EXACTVERSION CACHE STRING "exact version of python3")

macro(Python_BUILD_EXTENSION Python_BUILD_VERSION)

if(NOT ${Python${Python_BUILD_VERSION}_EXACTVERSION} STREQUAL "")
  find_package(Python${Python_BUILD_VERSION} ${Python${Python_BUILD_VERSION}_EXACTVERSION} EXACT REQUIRED COMPONENTS Interpreter Development)
else()
  find_package(Python${Python_BUILD_VERSION} REQUIRED COMPONENTS Interpreter Development)
endif()

find_program(CYTHON_EXECUTABLE cython)

# Figure out installation path
# CMake populates this var automatically but we need to specify prefix
execute_process(COMMAND
  ${Python${Python_BUILD_VERSION}_EXECUTABLE}
    -c "from distutils.sysconfig import get_python_lib; print(get_python_lib(plat_specific=False, standard_lib=False, prefix='${CMAKE_INSTALL_PREFIX}'))"
  OUTPUT_VARIABLE Python${Python_BUILD_VERSION}_SITELIB OUTPUT_STRIP_TRAILING_WHITESPACE)

# Figure out numpy include path
# todo: CMake >= 3.14 populates this var automatically when using COMPONENTS NumPy
execute_process(COMMAND
  ${Python${Python_BUILD_VERSION}_EXECUTABLE}
    -c "import numpy; print(numpy.get_include())"
  OUTPUT_VARIABLE Python${Python_BUILD_VERSION}_NumPy_INCLUDE_DIRS OUTPUT_STRIP_TRAILING_WHITESPACE)

# How to Cython the .pyx file
add_custom_command(OUTPUT freenect${Python_BUILD_VERSION}.c
  COMMAND
    ${CYTHON_EXECUTABLE}
      -${Python_BUILD_VERSION}
      -o freenect${Python_BUILD_VERSION}.c
      "${CMAKE_CURRENT_SOURCE_DIR}/freenect.pyx")
list(APPEND ADDITIONAL_MAKE_CLEAN_FILES freenect${Python_BUILD_VERSION}.c)

# Compile the extension
add_library(cython${Python_BUILD_VERSION}_freenect MODULE freenect${Python_BUILD_VERSION}.c)
set_target_properties(cython${Python_BUILD_VERSION}_freenect PROPERTIES
  PREFIX ""
  OUTPUT_NAME "freenect"
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/python${Python_BUILD_VERSION})
target_link_libraries(cython${Python_BUILD_VERSION}_freenect
  freenect_sync
  ${Python${Python_BUILD_VERSION}_LIBRARIES})
target_include_directories(cython${Python_BUILD_VERSION}_freenect PRIVATE
  ${Python${Python_BUILD_VERSION}_INCLUDE_DIRS}
  ../c_sync/
  ${Python${Python_BUILD_VERSION}_NumPy_INCLUDE_DIRS})

# Install the extension
install(TARGETS cython${Python_BUILD_VERSION}_freenect
  DESTINATION ${Python${Python_BUILD_VERSION}_SITELIB})

# TODO: decide on what to do with demo_ scripts and were to install
#       them
endmacro(Python_BUILD_EXTENSION)

if (BUILD_PYTHON2)
  Python_BUILD_EXTENSION(2)
endif(BUILD_PYTHON2)
if (BUILD_PYTHON3)
  Python_BUILD_EXTENSION(3)
endif(BUILD_PYTHON3)
