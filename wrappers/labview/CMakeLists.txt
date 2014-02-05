######################################################################################
# C Labview Interface
######################################################################################
set_source_files_properties(libfreenect_lv.c PROPERTIES LANGUAGE CXX)

add_library (freenect_lv SHARED libfreenect_lv.c)
set_target_properties (freenect_lv PROPERTIES
  VERSION ${PROJECT_VER}
  SOVERSION ${PROJECT_APIVER})

target_link_libraries (freenect_lv freenect freenect_sync)

install (TARGETS freenect_lv
  DESTINATION "${PROJECT_LIBRARY_INSTALL_DIR}")
install (FILES "libfreenect_lv.h"
  DESTINATION ${PROJECT_INCLUDE_INSTALL_DIR})