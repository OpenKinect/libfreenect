INSTALL(FILES libfreenect.hpp
  DESTINATION ${PROJECT_INCLUDE_INSTALL_DIR})

IF(BUILD_EXAMPLES)

if (WIN32)
  set(THREADS_USE_PTHREADS_WIN32 true)
  find_package(Threads REQUIRED)

  include_directories(${THREADS_PTHREADS_INCLUDE_DIR})
endif()

include_directories(.)

add_executable(freenect-cppview cppview.cpp)
add_executable(freenect-cpp_pcview cpp_pc_view.cpp)

# Mac just has everything already
if(APPLE)
  set(CMAKE_EXE_LINKER_FLAGS "-framework OpenGL -framework GLUT")
  target_link_libraries(freenect-cppview freenect)
  target_link_libraries(freenect-cpp_pcview freenect)
else()
  find_package(Threads REQUIRED)
  find_package(OpenGL REQUIRED)
  find_package(GLUT REQUIRED)

  include_directories(${OPENGL_INCLUDE_DIR} ${GLUT_INCLUDE_DIR} ${USB_INCLUDE_DIRS})

  target_link_libraries(freenect-cppview freenect ${OPENGL_LIBRARIES} ${GLUT_LIBRARY} ${CMAKE_THREAD_LIBS_INIT} ${MATH_LIB})
  target_link_libraries(freenect-cpp_pcview freenect ${OPENGL_LIBRARIES} ${GLUT_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})
endif()

install (TARGETS freenect-cppview
  DESTINATION bin)

install (TARGETS freenect-cpp_pcview
  DESTINATION bin)

ENDIF()
