######################################################################################
# Linux integration: udev rules, firmware upload at plug-in, modprobe example
######################################################################################
#
# Where udev and systemd look for these files depends on the install prefix:
#   prefix /usr        -> /usr/lib/udev/rules.d,  /usr/lib/systemd/system  (packages)
#   prefix /usr/local  -> /etc/udev/rules.d,      /usr/local/lib/systemd/system
#                         (udev does not read /usr/local; systemd does)
#   any other prefix   -> <prefix>/lib/udev/rules.d, <prefix>/lib/systemd/system,
#                         to be copied into place by hand
# LIBFREENECT_UDEV_RULES_DIR and LIBFREENECT_SYSTEMD_UNIT_DIR override this;
# relative paths are relative to the prefix. Packages (CPack) should be
# configured with -DCMAKE_INSTALL_PREFIX=/usr.

set (_udev_dir "lib/udev/rules.d")
set (_unit_dir "lib/systemd/system")
if (CMAKE_INSTALL_PREFIX STREQUAL "/usr/local")
  set (_udev_dir "/etc/udev/rules.d")
endif ()
set (LIBFREENECT_UDEV_RULES_DIR "${_udev_dir}" CACHE STRING "Where to install 51-kinect.rules")
set (LIBFREENECT_SYSTEMD_UNIT_DIR "${_unit_dir}" CACHE STRING "Where to install freenect-fwloader.service")

set (FWLOADER_PATH "${CMAKE_INSTALL_PREFIX}/bin/freenect-fwloader")
configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/systemd/freenect-fwloader.service.in"
                "${CMAKE_CURRENT_BINARY_DIR}/freenect-fwloader.service" @ONLY)

install (FILES "${CMAKE_CURRENT_SOURCE_DIR}/udev/51-kinect.rules"
         DESTINATION "${LIBFREENECT_UDEV_RULES_DIR}")
if (BUILD_EXAMPLES)
  # The unit runs freenect-fwloader, which is built with the examples.
  install (FILES "${CMAKE_CURRENT_BINARY_DIR}/freenect-fwloader.service"
           DESTINATION "${LIBFREENECT_SYSTEMD_UNIT_DIR}")
endif ()
install (FILES "${CMAKE_CURRENT_SOURCE_DIR}/modprobe.d/kinect.conf"
         "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
         DESTINATION "share/doc/libfreenect/linux")

message (STATUS "udev rules will be installed to ${LIBFREENECT_UDEV_RULES_DIR}")

# The beamformed microphone for PipeWire (needs the audio DSP)
if (BUILD_AUDIO_DSP)
  add_subdirectory (pipewire)
endif ()
