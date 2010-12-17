######################################################################################
# Packages needed for examples
######################################################################################

set(CMAKE_C_FLAGS "-Wall")

if (WIN32)
  set_source_files_properties(glview.c PROPERTIES LANGUAGE CXX)
  set_source_files_properties(glpclview.c PROPERTIES LANGUAGE CXX)

  set(THREADS_USE_PTHREADS_WIN32 true)
  find_package(Threads REQUIRED)

  include_directories(${THREADS_PTHREADS_INCLUDE_DIR})
endif()

add_executable(glview glview.c)
add_executable(cppview cppview.cpp)

if (BUILD_C_SYNC)
  add_executable(glpclview glpclview.c)
endif()

# We need to include libfreenect_sync.h for glpclview
include_directories (../wrappers/c_sync/)

# Mac just has everything already
if(APPLE)
  set(CMAKE_EXE_LINKER_FLAGS "-framework OpenGL -framework GLUT")
  target_link_libraries(glview freenect)
  target_link_libraries(cppview freenect)
  if (BUILD_C_SYNC)
    target_link_libraries(glpclview freenect_sync)
  endif()
# Linux, not so much
else()

  find_package(Threads REQUIRED)
  find_package(OpenGL REQUIRED)
  find_package(GLUT REQUIRED)

  include_directories(${OPENGL_INCLUDE_DIRS} ${GLUT_INCLUDE_DIRS} ${USB_INCLUDE_DIRS})

  if (WIN32)
    set(MATH_LIB "")
  else(WIN32)
    set(MATH_LIB "m")
  endif()

  target_link_libraries(glview freenect ${OPENGL_LIBRARIES} ${GLUT_LIBRARY} ${CMAKE_THREAD_LIBS_INIT} ${MATH_LIB})
  target_link_libraries(cppview freenect ${OPENGL_LIBRARIES} ${GLUT_LIBRARY} ${CMAKE_THREAD_LIBS_INIT} ${MATH_LIB})
  if (BUILD_C_SYNC)
    target_link_libraries(glpclview freenect_sync ${OPENGL_LIBRARIES} ${GLUT_LIBRARY} ${CMAKE_THREAD_LIBS_INIT} ${MATH_LIB})
  endif()
endif()


install (TARGETS glview 
  DESTINATION bin)

install (TARGETS cppview
  DESTINATION bin)

if (BUILD_C_SYNC)
  install (TARGETS glpclview
    DESTINATION bin)
endif()