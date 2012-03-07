######################################################################################
# Proxynect Persistent Kinect Library
######################################################################################
SET(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
SET(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib/proxynect)
add_library (proxynect SHARED proxynect.c proxyutils.c)
set_target_properties (proxynect PROPERTIES
  VERSION ${PROJECT_VER}
  SOVERSION ${PROJECT_APIVER}
  OUTPUT_NAME freenect)

install (TARGETS proxynect
  DESTINATION "${PROJECT_LIBRARY_INSTALL_DIR}/proxynect")

add_executable(proxydaemon proxydaemon.c proxyutils.c)
target_link_libraries(proxydaemon freenect m)
install (TARGETS proxydaemon
  DESTINATION bin)

CONFIGURE_FILE("proxynect.sh.in"
  "proxynect.sh"
  IMMEDIATE @ONLY)
install (PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/proxynect.sh
  DESTINATION bin
  RENAME proxynect)
