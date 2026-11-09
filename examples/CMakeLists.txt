######################################################################################
# Packages needed for examples
######################################################################################

cmake_minimum_required(VERSION 3.10)

# These examples have no external dependencies and should always build.
add_executable(freenect-camtest camtest.c)
add_executable(freenect-wavrecord wavrecord.c)
add_executable(freenect-fwloader fwloader.c)
add_executable(freenect-hotplug hotplug.c)
add_executable(freenect-maptest maptest.c)
target_link_libraries(freenect-camtest freenect)
target_link_libraries(freenect-wavrecord freenect)
target_link_libraries(freenect-fwloader freenect)
target_link_libraries(freenect-hotplug freenect)
target_link_libraries(freenect-maptest freenect)
install(TARGETS freenect-camtest freenect-wavrecord freenect-fwloader freenect-hotplug freenect-maptest
        DESTINATION bin)

# freenect-v4l2 (Linux): depth and colour as V4L2 cameras through v4l2loopback
if (PROJECT_OS_LINUX AND BUILD_V4L2_BRIDGE)
  include(CheckIncludeFile)
  check_include_file(linux/videodev2.h HAVE_LINUX_VIDEODEV2_H)
  if (HAVE_LINUX_VIDEODEV2_H)
    add_executable(freenect-v4l2 v4l2bridge.c)
    target_link_libraries(freenect-v4l2 freenect)
    install(TARGETS freenect-v4l2 DESTINATION bin)
  else ()
    message(STATUS "freenect-v4l2 not built: linux/videodev2.h not found (linux-libc-dev)")
  endif ()
endif ()

# The OpenGL viewers need pthreads, OpenGL, and GLFW or GLUT
# (FREENECT_GL_VIEWERS, cmake_modules/FreenectGLViewers.cmake).
set(THREADS_USE_PTHREADS_WIN32 true)
find_package(Threads)

if (Threads_FOUND AND FREENECT_GL_BACKEND)
  include_directories(${THREADS_PTHREADS_INCLUDE_DIR})
  foreach (viewer glview regview hiview chunkview micview)
    freenect_gl_viewer(freenect-${viewer} ${viewer}.c)
    target_link_libraries(freenect-${viewer} freenect ${CMAKE_THREAD_LIBS_INIT} ${MATH_LIB})
  endforeach ()
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

  if (FREENECT_GL_BACKEND AND FREENECT_GL_HAVE_GLU)
    freenect_gl_viewer(freenect-glpclview glpclview.c)
    target_link_libraries(freenect-glpclview freenect_sync ${MATH_LIB})
    install(TARGETS freenect-glpclview
            DESTINATION bin)
  endif ()
endif ()
