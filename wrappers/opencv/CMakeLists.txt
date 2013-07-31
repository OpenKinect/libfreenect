######################################################################################
# OpenCV Interface
######################################################################################
find_package(OpenCV REQUIRED)
add_library (freenect_cv SHARED libfreenect_cv.c)
set_target_properties (freenect_cv PROPERTIES
  VERSION ${PROJECT_VER}
  SOVERSION ${PROJECT_APIVER})

include_directories (../c_sync)

target_link_libraries (freenect_cv freenect_sync ${OpenCV_LIBS})

install (TARGETS freenect_cv
  DESTINATION "${PROJECT_LIBRARY_INSTALL_DIR}")
install (FILES "libfreenect_cv.h"
  DESTINATION ${PROJECT_INCLUDE_INSTALL_DIR})

add_executable(cvdemo cvdemo.c)
target_link_libraries(cvdemo freenect freenect_sync freenect_cv ${OpenCV_LIBS})
install (TARGETS cvdemo
  DESTINATION bin)
