INSTALL(FILES libfreenect.hpp
  DESTINATION ${PROJECT_INCLUDE_INSTALL_DIR})

IF(BUILD_EXAMPLES)

set(CMAKE_C_FLAGS "-Wall")

if (WIN32)
  set(THREADS_USE_PTHREADS_WIN32 true)
  find_package(Threads REQUIRED)

  include_directories(${THREADS_PTHREADS_INCLUDE_DIR})
endif()

include_directories(.)

add_executable(freenect-cppview cppview.cpp)

# Mac just has everything already
if(APPLE)
  set(CMAKE_EXE_LINKER_FLAGS "-framework OpenGL -framework GLUT")
  target_link_libraries(freenect-cppview freenect)
else()
  find_package(Threads REQUIRED)
  find_package(OpenGL REQUIRED)
  find_package(GLUT REQUIRED)

  include_directories(${OPENGL_INCLUDE_DIR} ${GLUT_INCLUDE_DIR} ${USB_INCLUDE_DIRS})

  target_link_libraries(freenect-cppview freenect ${OPENGL_LIBRARIES} ${GLUT_LIBRARY} ${CMAKE_THREAD_LIBS_INIT} ${MATH_LIB})
endif()

install (TARGETS freenect-cppview
  DESTINATION bin)

ENDIF()
