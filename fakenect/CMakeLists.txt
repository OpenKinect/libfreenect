######################################################################################
# Fakenect Mock Library
######################################################################################
SET(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
SET(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib/fakenect)
add_library (fakenect SHARED fakenect.c)
set_target_properties ( fakenect PROPERTIES
  VERSION ${PROJECT_VER}
  SOVERSION ${PROJECT_APIVER}
  OUTPUT_NAME freenect)

install (TARGETS fakenect
  DESTINATION "${PROJECT_LIBRARY_INSTALL_DIR}/fakenect")

add_executable(record record.c)
target_link_libraries(record freenect m)
install (TARGETS record
  DESTINATION bin)

CONFIGURE_FILE("fakenect.sh.in"
  "fakenect.sh"
  IMMEDIATE @ONLY)
install (PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/fakenect.sh
  DESTINATION bin
  RENAME fakenect)
