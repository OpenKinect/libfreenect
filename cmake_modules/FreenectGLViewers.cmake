# Window library of the OpenGL viewers (glview, regview, ...).
#
#   FREENECT_GL_VIEWERS=AUTO  GLFW if found, else GLUT (the default)
#   FREENECT_GL_VIEWERS=GLFW  GLFW, or fail
#   FREENECT_GL_VIEWERS=GLUT  GLUT (freeglut, or the GLUT framework on macOS)
#   FREENECT_GL_VIEWERS=OFF   no viewers
#
# GLFW runs natively on Wayland as well as on X11, macOS and Windows; the
# viewers reach it through examples/fn_glut.h, which implements the GLUT calls
# they use. Sets FREENECT_GL_BACKEND (GLFW, GLUT or empty) and
# FREENECT_GL_HAVE_GLU, and defines freenect_gl_viewer(target sources...).
# Included once from the top-level CMakeLists.txt, so that examples/ and
# wrappers/cpp/ both see the result.

set(FREENECT_GL_VIEWERS "AUTO" CACHE STRING "Window library of the OpenGL viewers: AUTO, GLFW, GLUT or OFF")
set_property(CACHE FREENECT_GL_VIEWERS PROPERTY STRINGS AUTO GLFW GLUT OFF)
set(_fn_examples_dir "${CMAKE_CURRENT_LIST_DIR}/../examples")

set(FREENECT_GL_BACKEND "")
if (NOT FREENECT_GL_VIEWERS STREQUAL "OFF")
  find_package(OpenGL COMPONENTS OpenGL)
  if (OpenGL_OpenGL_FOUND)
    if (FREENECT_GL_VIEWERS STREQUAL "AUTO" OR FREENECT_GL_VIEWERS STREQUAL "GLFW")
      find_package(glfw3 3.3 CONFIG QUIET)
      if (glfw3_FOUND)
        set(FREENECT_GL_BACKEND GLFW)
      elseif (FREENECT_GL_VIEWERS STREQUAL "GLFW")
        message(FATAL_ERROR "FREENECT_GL_VIEWERS=GLFW, but GLFW 3.3 or newer was not found (libglfw3-dev, glfw)")
      endif ()
    endif ()
    if (NOT FREENECT_GL_BACKEND AND (FREENECT_GL_VIEWERS STREQUAL "AUTO" OR FREENECT_GL_VIEWERS STREQUAL "GLUT"))
      find_package(GLUT)
      if (GLUT_FOUND)
        set(FREENECT_GL_BACKEND GLUT)
      elseif (FREENECT_GL_VIEWERS STREQUAL "GLUT")
        message(FATAL_ERROR "FREENECT_GL_VIEWERS=GLUT, but GLUT was not found (freeglut3-dev, freeglut)")
      endif ()
    endif ()
  elseif (NOT FREENECT_GL_VIEWERS STREQUAL "AUTO")
    message(FATAL_ERROR "FREENECT_GL_VIEWERS=${FREENECT_GL_VIEWERS}, but OpenGL was not found")
  endif ()
endif ()

set(FREENECT_GL_HAVE_GLU OFF)
if (FREENECT_GL_BACKEND AND TARGET OpenGL::GLU)
  set(FREENECT_GL_HAVE_GLU ON)
endif ()

if (FREENECT_GL_BACKEND)
  include(ChooseOpenGLTarget)
endif ()
if (FREENECT_GL_BACKEND)
  message(STATUS "OpenGL viewers: ${FREENECT_GL_BACKEND}")
else ()
  message(STATUS "OpenGL viewers: not built (they need OpenGL and GLFW, or GLUT)")
endif ()

# freenect_gl_viewer(target source...): an executable using fn_glut.h.
# Link libfreenect and threads to it as usual.
function(freenect_gl_viewer target)
  if (FREENECT_GL_BACKEND STREQUAL "GLFW")
    add_executable(${target} ${ARGN} "${_fn_examples_dir}/fn_glut.c")
    target_compile_definitions(${target} PRIVATE FN_USE_GLFW)
    target_link_libraries(${target} glfw "${OPENGL_TARGET}")
  else ()
    add_executable(${target} ${ARGN})
    target_link_libraries(${target} "${OPENGL_TARGET}" GLUT::GLUT)
  endif ()
  if (FREENECT_GL_HAVE_GLU)
    target_compile_definitions(${target} PRIVATE FN_GLUT_GLU)
    target_link_libraries(${target} OpenGL::GLU)
  endif ()
  target_include_directories(${target} PRIVATE "${_fn_examples_dir}" ${OPENGL_INCLUDE_DIR})
endfunction()
