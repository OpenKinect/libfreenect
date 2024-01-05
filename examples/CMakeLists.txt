######################################################################################
# Packages needed for examples
######################################################################################

cmake_minimum_required(VERSION 3.10)

# These examples have no external dependencies and should always build.
add_executable(freenect-camtest camtest.c)
add_executable(freenect-wavrecord wavrecord.c)
target_link_libraries(freenect-camtest freenect)
target_link_libraries(freenect-wavrecord freenect)
install(TARGETS freenect-camtest freenect-wavrecord
        DESTINATION bin)

# Most viewers need pthreads and GLUT.
set(THREADS_USE_PTHREADS_WIN32 true)
find_package(Threads)
find_package(OpenGL COMPONENTS OpenGL)
find_package(GLUT)

if (Threads_FOUND AND OpenGL_OpenGL_FOUND AND GLUT_FOUND)
  include_directories(${THREADS_PTHREADS_INCLUDE_DIR} ${OPENGL_INCLUDE_DIR} ${GLUT_INCLUDE_DIR})

  add_executable(freenect-glview glview.c)
  add_executable(freenect-regview regview.c)
  add_executable(freenect-hiview hiview.c)
  add_executable(freenect-chunkview chunkview.c)
  add_executable(freenect-micview micview.c)

  include(ChooseOpenGLTarget)
  target_link_libraries(freenect-glview freenect "${OPENGL_TARGET}" GLUT::GLUT ${CMAKE_THREAD_LIBS_INIT} ${MATH_LIB})
  target_link_libraries(freenect-regview freenect "${OPENGL_TARGET}" GLUT::GLUT ${CMAKE_THREAD_LIBS_INIT} ${MATH_LIB})
  target_link_libraries(freenect-hiview freenect "${OPENGL_TARGET}" GLUT::GLUT ${CMAKE_THREAD_LIBS_INIT} ${MATH_LIB})
  target_link_libraries(freenect-chunkview freenect "${OPENGL_TARGET}" GLUT::GLUT ${CMAKE_THREAD_LIBS_INIT} ${MATH_LIB})
  target_link_libraries(freenect-micview freenect "${OPENGL_TARGET}" GLUT::GLUT ${CMAKE_THREAD_LIBS_INIT} ${MATH_LIB})

  install(TARGETS freenect-glview freenect-regview freenect-hiview freenect-chunkview freenect-micview
          DESTINATION bin)
endif ()

# A few examples use c_sync.
if (BUILD_C_SYNC)
  include_directories(../wrappers/c_sync/)

  add_executable(freenect-regtest regtest.c)
  add_executable(freenect-tiltdemo tiltdemo.c)

  target_link_libraries(freenect-regtest freenect_sync ${MATH_LIB})
  target_link_libraries(freenect-tiltdemo freenect_sync ${MATH_LIB})

  install(TARGETS freenect-regtest freenect-tiltdemo
          DESTINATION bin)

  if (OpenGL_OpenGL_FOUND AND GLUT_FOUND)
    include_directories(${OPENGL_INCLUDE_DIR} ${GLUT_INCLUDE_DIR})
    add_executable(freenect-glpclview glpclview.c)
    include(ChooseOpenGLTarget)
    target_link_libraries(freenect-glpclview freenect_sync "${OPENGL_TARGET}" GLUT::GLUT ${MATH_LIB})
    install(TARGETS freenect-glpclview
            DESTINATION bin)
  endif ()
endif ()
