######################################################################################
# Packages needed for examples
######################################################################################

set(CMAKE_C_FLAGS "-Wall")

if (WIN32)
  set_source_files_properties(glview.c PROPERTIES LANGUAGE CXX)
  set_source_files_properties(regview.c PROPERTIES LANGUAGE CXX)
  set_source_files_properties(glpclview.c PROPERTIES LANGUAGE CXX)
  set_source_files_properties(hiview.c PROPERTIES LANGUAGE CXX)
  set_source_files_properties(tiltdemo.c PROPERTIES LANGUAGE CXX)
  set_source_files_properties(chunkview.c PROPERTIES LANGUAGE CXX)

  set(THREADS_USE_PTHREADS_WIN32 true)
  find_package(Threads REQUIRED)

  include_directories(${THREADS_PTHREADS_INCLUDE_DIR})
endif()

add_executable(freenect-glview glview.c)
add_executable(freenect-regview regview.c)
add_executable(freenect-hiview hiview.c)
add_executable(freenect-chunkview chunkview.c)
if(BUILD_AUDIO)
  add_executable(freenect-wavrecord wavrecord.c)
  add_executable(freenect-micview micview.c)
endif()

if (BUILD_C_SYNC)
  add_executable(freenect-glpclview glpclview.c)
  add_executable(freenect-tiltdemo tiltdemo.c)
  add_executable(freenect-regtest regtest.c)
endif()

# We need to include libfreenect_sync.h for glpclview
include_directories (../wrappers/c_sync/)

# Mac just has everything already
if(APPLE)
  set(CMAKE_EXE_LINKER_FLAGS "-framework OpenGL -framework GLUT")
  target_link_libraries(freenect-glview freenect)
  target_link_libraries(freenect-regview freenect)
  target_link_libraries(freenect-hiview freenect)
  target_link_libraries(freenect-chunkview freenect)
  if (BUILD_AUDIO)
    target_link_libraries(freenect-wavrecord freenect)
    target_link_libraries(freenect-micview freenect)
  endif()
  if (BUILD_C_SYNC)
    target_link_libraries(freenect-glpclview freenect_sync)
    target_link_libraries(freenect-tiltdemo freenect_sync)
    target_link_libraries(freenect-regtest freenect_sync)
  endif()
# Linux, not so much
else()

  find_package(Threads REQUIRED)
  find_package(OpenGL REQUIRED)
  find_package(GLUT REQUIRED)

  include_directories(${OPENGL_INCLUDE_DIR} ${GLUT_INCLUDE_DIR} ${USB_INCLUDE_DIRS})

  target_link_libraries(freenect-glview freenect ${OPENGL_LIBRARIES} ${GLUT_LIBRARY} ${CMAKE_THREAD_LIBS_INIT} ${MATH_LIB})
  target_link_libraries(freenect-regview freenect ${OPENGL_LIBRARIES} ${GLUT_LIBRARY} ${CMAKE_THREAD_LIBS_INIT} ${MATH_LIB})
  target_link_libraries(freenect-hiview freenect ${OPENGL_LIBRARIES} ${GLUT_LIBRARY} ${CMAKE_THREAD_LIBS_INIT} ${MATH_LIB})
  target_link_libraries(freenect-chunkview freenect ${OPENGL_LIBRARIES} ${GLUT_LIBRARY} ${CMAKE_THREAD_LIBS_INIT} ${MATH_LIB})
  if (BUILD_AUDIO)
    target_link_libraries(freenect-wavrecord freenect ${OPENGL_LIBRARIES} ${GLUT_LIBRARY} ${CMAKE_THREAD_LIBS_INIT} ${MATH_LIB})
    target_link_libraries(freenect-micview freenect ${OPENGL_LIBRARIES} ${GLUT_LIBRARY} ${CMAKE_THREAD_LIBS_INIT} ${MATH_LIB})
  endif()
  if (BUILD_C_SYNC)
    target_link_libraries(freenect-glpclview freenect_sync ${OPENGL_LIBRARIES} ${GLUT_LIBRARY}
 ${CMAKE_THREAD_LIBS_INIT} ${MATH_LIB})
    target_link_libraries(freenect-tiltdemo freenect_sync ${CMAKE_THREAD_LIBS_INIT} ${MATH_LIB})
    target_link_libraries(freenect-regtest freenect_sync ${CMAKE_THREAD_LIBS_INIT})
  endif()
endif()

install (TARGETS freenect-glview freenect-regview freenect-hiview freenect-chunkview
  DESTINATION bin)

if (BUILD_C_SYNC)
  install (TARGETS freenect-glpclview freenect-tiltdemo
    DESTINATION bin)
endif()

if (BUILD_AUDIO)
  install (TARGETS freenect-wavrecord DESTINATION bin)
  install (TARGETS freenect-micview DESTINATION bin)
endif()
