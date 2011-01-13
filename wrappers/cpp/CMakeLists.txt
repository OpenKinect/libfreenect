INSTALL(FILES libfreenect.hpp
  DESTINATION include)

IF(BUILD_EXAMPLES)

set(CMAKE_C_FLAGS "-Wall")

if (WIN32)
  set(THREADS_USE_PTHREADS_WIN32 true)
  find_package(Threads REQUIRED)

  include_directories(${THREADS_PTHREADS_INCLUDE_DIR})
endif()

include_directories(.)

add_executable(cppview cppview.cpp)

# Mac just has everything already
if(APPLE)
  set(CMAKE_EXE_LINKER_FLAGS "-framework OpenGL -framework GLUT")
  target_link_libraries(cppview freenect)
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

  target_link_libraries(cppview freenect ${OPENGL_LIBRARIES} ${GLUT_LIBRARY} ${CMAKE_THREAD_LIBS_INIT} ${MATH_LIB})
endif()

install (TARGETS cppview
  DESTINATION bin)

ENDIF()
