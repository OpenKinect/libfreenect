######################################################################################
# Build for main library
######################################################################################

include_directories (${CMAKE_CURRENT_SOURCE_DIR})

set(CMAKE_C_FLAGS "-Wall")

include_directories(${LIBUSB_1_INCLUDE_DIRS})
IF(WIN32)
  LIST(APPEND SRC core.c tilt.c cameras.c usb_libusb10.c registration.c ../platform/windows/libusb10emu/libusb-1.0/libusbemu.cpp ../platform/windows/libusb10emu/libusb-1.0/failguard.cpp)
  set_source_files_properties(${SRC} PROPERTIES LANGUAGE CXX)
ELSE(WIN32)
  LIST(APPEND SRC core.c tilt.c cameras.c usb_libusb10.c registration.c)
ENDIF(WIN32)

IF(BUILD_AUDIO)
  include(FindPythonInterp)
  LIST(APPEND SRC audio.c loader.c)
  IF(BUILD_REDIST_PACKAGE)
    # If this build is intended for a redistributable package, we can't include audios.bin, so we should include fwfetcher.py
    # and the package should run "python fwfetcher.py $INSTALL_PREFIX/share" as a postinst hook
    install (FILES "fwfetcher.py" DESTINATION "${CMAKE_INSTALL_PREFIX}/share")
  ELSE(BUILD_REDIST_PACKAGE)
    # If the install is local only, we can just run fwfetcher.py and install the audios.bin firmware to the system folder
    add_custom_target(firmware ALL
        COMMAND ${PYTHON_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/fwfetcher.py" "../audios.bin"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/fwfetcher.py")
    install (FILES "${CMAKE_CURRENT_BINARY_DIR}/../audios.bin" DESTINATION "${CMAKE_INSTALL_PREFIX}/share")
  ENDIF()
ENDIF()

add_library (freenect SHARED ${SRC})
set_target_properties ( freenect PROPERTIES
  VERSION ${PROJECT_VER}
  SOVERSION ${PROJECT_APIVER})	
install (TARGETS freenect
  DESTINATION "${PROJECT_LIBRARY_INSTALL_DIR}")

add_library (freenectstatic STATIC ${SRC})
set_target_properties (freenectstatic PROPERTIES OUTPUT_NAME freenect)
IF(UNIX AND NOT APPLE)
  SET_TARGET_PROPERTIES (freenectstatic PROPERTIES COMPILE_FLAGS "-fPIC")
ENDIF()
install (TARGETS freenectstatic
  DESTINATION "${PROJECT_LIBRARY_INSTALL_DIR}")

target_link_libraries (freenect ${LIBUSB_1_LIBRARIES})
target_link_libraries (freenectstatic ${LIBUSB_1_LIBRARIES})

# Install the header files
install (FILES "../include/libfreenect.h" "../include/libfreenect-registration.h"
  DESTINATION ${PROJECT_INCLUDE_INSTALL_DIR})
if(BUILD_AUDIO)
  install (FILES "../include/libfreenect-audio.h"
    DESTINATION ${PROJECT_INCLUDE_INSTALL_DIR})
endif()

IF(UNIX AND NOT APPLE)
  # Produce a pkg-config file for linking against the shared lib
  configure_file ("libfreenect.pc.in" "libfreenect.pc" @ONLY)
  install (FILES "${CMAKE_CURRENT_BINARY_DIR}/libfreenect.pc"
    DESTINATION "${PROJECT_LIBRARY_INSTALL_DIR}/pkgconfig")
ENDIF()
