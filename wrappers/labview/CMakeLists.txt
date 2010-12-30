######################################################################################
# C Labview Interface
######################################################################################
set_source_files_properties(libfreenect_lv.c PROPERTIES LANGUAGE CXX)
set_source_files_properties(lvdemo.c PROPERTIES LANGUAGE CXX)

add_library (freenect_lv SHARED libfreenect_lv.c)
set_target_properties (freenect_lv PROPERTIES
  VERSION ${PROJECT_VER}
  SOVERSION ${PROJECT_APIVER})

target_link_libraries (freenect_lv freenect freenect_sync)

install (TARGETS freenect_lv
  DESTINATION "${PROJECT_LIBRARY_INSTALL_DIR}")
install (FILES "libfreenect_lv.h"
  DESTINATION ${PROJECT_INCLUDE_INSTALL_DIR})

  set(THREADS_USE_PTHREADS_WIN32 true)
  find_package(Threads REQUIRED)

  include_directories(${THREADS_PTHREADS_INCLUDE_DIR})

  find_package(Threads REQUIRED)
  find_package(OpenGL REQUIRED)
  find_package(GLUT REQUIRED)

  include_directories(${OPENGL_INCLUDE_DIRS} ${GLUT_INCLUDE_DIRS} ${USB_INCLUDE_DIRS})

  if (WIN32)
    set(MATH_LIB "")
  else(WIN32)
    set(MATH_LIB "m")
  endif()

add_executable(lvdemo lvdemo.c)

target_link_libraries(lvdemo freenect freenect_sync freenect_lv ${OPENGL_LIBRARIES} ${GLUT_LIBRARY} ${CMAKE_THREAD_LIBS_INIT} ${MATH_LIB})

install (TARGETS lvdemo
  DESTINATION bin)