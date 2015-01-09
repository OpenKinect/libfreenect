install(FILES libfreenect.hpp
        DESTINATION ${PROJECT_INCLUDE_INSTALL_DIR})

if (BUILD_EXAMPLES)
  set(THREADS_USE_PTHREADS_WIN32 true)
  find_package(Threads REQUIRED)
  find_package(OpenGL REQUIRED)
  find_package(GLUT REQUIRED)

  include_directories(. ${THREADS_PTHREADS_INCLUDE_DIR} ${OPENGL_INCLUDE_DIR} ${GLUT_INCLUDE_DIR})

  add_executable(freenect-cppview cppview.cpp)
  add_executable(freenect-cpp_pcview cpp_pc_view.cpp)

  target_link_libraries(freenect-cppview freenect ${OPENGL_LIBRARIES} ${GLUT_LIBRARY} ${CMAKE_THREAD_LIBS_INIT} ${MATH_LIB})
  target_link_libraries(freenect-cpp_pcview freenect ${OPENGL_LIBRARIES} ${GLUT_LIBRARY} ${CMAKE_THREAD_LIBS_INIT} ${MATH_LIB})

  install(TARGETS freenect-cppview freenect-cpp_pcview
          DESTINATION bin)
ENDIF()
