######################################################################################
# Fakenect Mock Library
######################################################################################
SET(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
SET(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib/fakenect)
include_directories(../src)
add_library (fakenect SHARED fakenect.c parson.c ../src/registration.c ../src/coordinate_mapper.c)
# no fused multiply-add in the mapper (see src/CMakeLists.txt)
if (NOT MSVC)
  set_source_files_properties (../src/coordinate_mapper.c PROPERTIES COMPILE_OPTIONS "-ffp-contract=off")
endif ()
set_target_properties ( fakenect PROPERTIES
  VERSION ${PROJECT_VER}
  SOVERSION ${PROJECT_APIVER}
  OUTPUT_NAME fakenect)
target_link_libraries(fakenect ${MATH_LIB})

install (TARGETS fakenect
  DESTINATION "${PROJECT_LIBRARY_INSTALL_DIR}/fakenect")

add_executable(fakenect-record record.c parson.c)
target_link_libraries(fakenect-record freenect ${MATH_LIB})
install (TARGETS fakenect-record
  DESTINATION bin)

CONFIGURE_FILE("fakenect.sh.in"
  "fakenect.sh"
  IMMEDIATE @ONLY)
install (PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/fakenect.sh
  DESTINATION bin
  RENAME fakenect)

install(FILES fakenect.1 fakenect-record.1
  DESTINATION ${PROJECT_MANPAGE_INSTALL_DIR}/man1)
