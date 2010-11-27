######################################################################################
# C Synchronous Interface
######################################################################################
add_library (freenect_sync SHARED libfreenect_sync.c)
set_target_properties (freenect_sync PROPERTIES
  VERSION ${PROJECT_VER}
  SOVERSION ${PROJECT_APIVER})

include_directories(${PTHREADS_INCLUDE_DIR})

IF(WIN32)
target_link_libraries (freenect_sync libfreenectstatic ${PTHREADS_LIBRARY})
ELSE()
target_link_libraries (freenect_sync freenect)
ENDIF()

install (TARGETS freenect_sync
  DESTINATION "${PROJECT_LIBRARY_INSTALL_DIR}")
install (FILES "libfreenect_sync.h"
  DESTINATION ${PROJECT_INCLUDE_INSTALL_DIR})