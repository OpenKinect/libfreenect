######################################################################################
# Python extension builder
######################################################################################

include(FindPythonInterp)
include(FindPythonLibs)

find_program(CYTHON_EXECUTABLE cython)

# Figure out installation path
execute_process(COMMAND
  ${PYTHON_EXECUTABLE} -c "from distutils.sysconfig import get_python_lib; print get_python_lib(prefix='${CMAKE_INSTALL_PREFIX}')"
  OUTPUT_VARIABLE PYTHON_SITE_PACKAGES OUTPUT_STRIP_TRAILING_WHITESPACE)

# Figure out numpy include path
execute_process(COMMAND
  ${PYTHON_EXECUTABLE} -c "import numpy; print numpy.get_include()"
  OUTPUT_VARIABLE NUMPY_INCLUDE_PATH OUTPUT_STRIP_TRAILING_WHITESPACE)

# How to Cython the .pyx file
add_custom_command(OUTPUT freenect.c
  COMMAND ${CYTHON_EXECUTABLE} -o freenect.c "${CMAKE_CURRENT_SOURCE_DIR}/freenect.pyx")
list(APPEND ADDITIONAL_MAKE_CLEAN_FILES freenect.c)

# Compile the extension
add_library(cython_freenect MODULE freenect.c)
set_target_properties(cython_freenect PROPERTIES
  PREFIX ""
  OUTPUT_NAME "freenect"
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(cython_freenect freenect_sync)
include_directories(${PYTHON_INCLUDE_PATH} ../c_sync/ ${NUMPY_INCLUDE_PATH})

# Install the extension
install(TARGETS cython_freenect
  DESTINATION ${PYTHON_SITE_PACKAGES})

# TODO: decide on what to do with demo_ scripts and were to install
#       them
