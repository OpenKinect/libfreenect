cmake_minimum_required(VERSION 3.10)

install(FILES libfreenect.hpp
        DESTINATION ${PROJECT_INCLUDE_INSTALL_DIR})

if (BUILD_EXAMPLES)
  set(THREADS_USE_PTHREADS_WIN32 true)
  find_package(Threads)

  if (Threads_FOUND AND FREENECT_GL_BACKEND)
    include_directories(. ${THREADS_PTHREADS_INCLUDE_DIR})

    freenect_gl_viewer(freenect-cppview cppview.cpp)
    target_compile_features(freenect-cppview PUBLIC cxx_std_11)
    target_link_libraries(freenect-cppview freenect ${CMAKE_THREAD_LIBS_INIT} ${MATH_LIB})
    install(TARGETS freenect-cppview
            DESTINATION bin)

    if (FREENECT_GL_HAVE_GLU)
      freenect_gl_viewer(freenect-cpp_pcview cpp_pc_view.cpp)
      target_compile_features(freenect-cpp_pcview PUBLIC cxx_std_11)
      target_link_libraries(freenect-cpp_pcview freenect ${CMAKE_THREAD_LIBS_INIT} ${MATH_LIB})
      install(TARGETS freenect-cpp_pcview
              DESTINATION bin)
    endif()
  endif()
endif()
