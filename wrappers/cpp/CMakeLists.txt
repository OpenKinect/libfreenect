cmake_minimum_required(VERSION 3.10)

install(FILES libfreenect.hpp
        DESTINATION ${PROJECT_INCLUDE_INSTALL_DIR})

if (BUILD_EXAMPLES)
  set(THREADS_USE_PTHREADS_WIN32 true)
  find_package(Threads)
  find_package(OpenGL COMPONENTS OpenGL)
  find_package(GLUT)

  if (Threads_FOUND AND OpenGL_OpenGL_FOUND AND OPENGL_GLU_FOUND AND GLUT_FOUND)
    include_directories(. ${THREADS_PTHREADS_INCLUDE_DIR} ${OPENGL_INCLUDE_DIR} ${GLUT_INCLUDE_DIR})

    add_executable(freenect-cppview cppview.cpp)
    add_executable(freenect-cpp_pcview cpp_pc_view.cpp)

    target_compile_features(freenect-cppview PUBLIC cxx_std_11)
    target_compile_features(freenect-cpp_pcview PUBLIC cxx_std_11)

    include(ChooseOpenGLTarget)
    target_link_libraries(freenect-cppview freenect "${OPENGL_TARGET}" OpenGL::GLU GLUT::GLUT ${CMAKE_THREAD_LIBS_INIT} ${MATH_LIB})
    target_link_libraries(freenect-cpp_pcview freenect "${OPENGL_TARGET}" OpenGL::GLU GLUT::GLUT ${CMAKE_THREAD_LIBS_INIT} ${MATH_LIB})

    install(TARGETS freenect-cppview freenect-cpp_pcview
            DESTINATION bin)
  endif()
endif()
