######################################################################################
# Build for main library
######################################################################################

include_directories (${CMAKE_CURRENT_SOURCE_DIR})

# Doesn't work on windows yet.
IF(NOT UNIX)
  MESSAGE(FATAL_ERROR "libfreenect is not currently supported on this platform")
ENDIF()

set(CMAKE_C_FLAGS "-Wall")
find_package(Threads REQUIRED)
find_package(libusb-1.0 REQUIRED)
include_directories(${LIBUSB_1_INCLUDE_DIRS})
LIST(APPEND SRC usb_libusb10.c)

LIST(APPEND SRC core.c tilt.c cameras.c usb_libusb10.c)

if (NOT PROJECT_OS_WIN)
  add_library (freenect SHARED ${SRC})
  set_target_properties ( freenect PROPERTIES
    VERSION ${PROJECT_VER}
    SOVERSION ${PROJECT_APIVER})	
  install (TARGETS freenect
    DESTINATION "${PROJECT_LIBRARY_INSTALL_DIR}")
endif (NOT PROJECT_OS_WIN)

add_library (freenectstatic STATIC ${SRC})
set_target_properties (freenectstatic PROPERTIES OUTPUT_NAME freenect)
IF(UNIX AND NOT APPLE)
  SET_TARGET_PROPERTIES (freenectstatic PROPERTIES COMPILE_FLAGS "-fPIC")
ENDIF()
install (TARGETS freenectstatic
  DESTINATION "${PROJECT_LIBRARY_INSTALL_DIR}")

find_library (PTHREAD pthread)
target_link_libraries (freenect ${CMAKE_THREAD_LIBS_INIT} ${LIBUSB_1_LIBRARIES})
target_link_libraries (freenectstatic ${CMAKE_THREAD_LIBS_INIT} ${LIBUSB_1_LIBRARIES})

# Install the header file
install (FILES "../include/libfreenect.h" "../include/libfreenect.hpp"
  DESTINATION ${PROJECT_INCLUDE_INSTALL_DIR})
