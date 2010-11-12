# - Try to find libusb-1.0
# Once done, this will define
#
#  USB_FOUND - system has libusb-1.0
#  USB_INCLUDE_DIRS - the libusb-1.0 include directories
#  USB_LIBRARIES - link these to use libusb-1.0

include(LibFindMacros)

# Dependencies

# Use pkg-config to get hints about paths
libfind_pkg_check_modules(USB_PKGCONF libusb-1.0>=1.0.3)

# Include dir
find_path(USB_INCLUDE_DIR
  NAMES libusb.h
  PATHS ${USB_PKGCONF_INCLUDE_DIRS}
)

# Finally the library itself
find_library(USB_LIBRARY
  NAMES usb-1.0
  PATHS ${USB_PKGCONF_LIBRARY_DIRS}
)

# Set the include dir variables and the libraries and let libfind_process do the rest.
# NOTE: Singular variables for this library, plural for libraries this this lib depends on.
set(USB_PROCESS_INCLUDES USB_INCLUDE_DIR)
set(USB_PROCESS_LIBS USB_LIBRARY)
libfind_process(USB)
