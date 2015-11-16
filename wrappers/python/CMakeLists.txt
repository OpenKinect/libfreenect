######################################################################################
# Python extension builder
######################################################################################

macro(Python_BUILD_EXTENSION Python_BUILD_VERSION)

set(Python_ADDITIONAL_VERSIONS ${Python_BUILD_VERSION})
find_package(PythonInterp)
set(PYTHON${Python_BUILD_VERSION}INTERP_FOUND    ${PYTHONINTERP_FOUND})
set(PYTHON${Python_BUILD_VERSION}_EXECUTABLE     ${PYTHON_EXECUTABLE})
set(PYTHON${Python_BUILD_VERSION}_VERSION_STRING ${PYTHON_VERSION_STRING})
set(PYTHON${Python_BUILD_VERSION}_VERSION_MAJOR  ${PYTHON_VERSION_MAJOR})
set(PYTHON${Python_BUILD_VERSION}_VERSION_MINOR  ${PYTHON_VERSION_MINOR})
set(PYTHON${Python_BUILD_VERSION}_VERSION_PATCH  ${PYTHON_VERSION_PATCH})
find_package(PythonLibs)
set(PYTHON${Python_BUILD_VERSION}LIBS_FOUND          ${PYTHONLIBS_FOUND})
set(PYTHON${Python_BUILD_VERSION}LIBS_VERSION_STRING ${PYTHONLIBS_VERSION_STRING})
set(PYTHON${Python_BUILD_VERSION}_DEBUG_LIBRARIES    ${PYTHON_DEBUG_LIBRARIES})
set(PYTHON${Python_BUILD_VERSION}_INCLUDE_PATH       ${PYTHON_INCLUDE_PATH})
set(PYTHON${Python_BUILD_VERSION}_INCLUDE_DIRS       ${PYTHON_INCLUDE_DIRS})
set(PYTHON${Python_BUILD_VERSION}_LIBRARIES          ${PYTHON_LIBRARIES})
find_program(CYTHON_EXECUTABLE cython)

# Figure out installation path
execute_process(COMMAND
  ${PYTHON${Python_BUILD_VERSION}_EXECUTABLE}
    -c "from distutils.sysconfig import get_python_lib; print(get_python_lib(prefix='${CMAKE_INSTALL_PREFIX}'))"
  OUTPUT_VARIABLE PYTHON${Python_BUILD_VERSION}_SITE_PACKAGES OUTPUT_STRIP_TRAILING_WHITESPACE)

# Figure out numpy include path
execute_process(COMMAND
  ${PYTHON${Python_BUILD_VERSION}_EXECUTABLE}
    -c "import numpy; print(numpy.get_include())"
  OUTPUT_VARIABLE PYTHON${Python_BUILD_VERSION}_NUMPY_INCLUDE_PATH OUTPUT_STRIP_TRAILING_WHITESPACE)

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
  ${PYTHON${Python_BUILD_VERSION}_LIBRARIES})
target_include_directories(cython${Python_BUILD_VERSION}_freenect PRIVATE
  ${PYTHON${Python_BUILD_VERSION}_INCLUDE_PATH}
  ../c_sync/
  ${PYTHON${Python_BUILD_VERSION}_NUMPY_INCLUDE_PATH})

# Install the extension
install(TARGETS cython${Python_BUILD_VERSION}_freenect
  DESTINATION ${PYTHON${Python_BUILD_VERSION}_SITE_PACKAGES})

# TODO: decide on what to do with demo_ scripts and were to install
#       them
endmacro(Python_BUILD_EXTENSION)

if (BUILD_PYTHON2)
  Python_BUILD_EXTENSION(2)
endif(BUILD_PYTHON2)
if (BUILD_PYTHON3)
  Python_BUILD_EXTENSION(3)
endif(BUILD_PYTHON3)
