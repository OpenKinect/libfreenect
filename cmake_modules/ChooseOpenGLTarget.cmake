# For maximum compatibility, choose the OpenGL target dynamically.
# On Linux with GLVND
#   OpenGL::OpenGL gives us just what we need without GLX or EGL
#   OpenGL::GL works as a fallback, but only if GLX is available
# On Linux without GLVND, OSX, Windows
#   OpenGL::GL is the only option
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.12)
  set(OPENGL_TARGET "$<IF:$<TARGET_EXISTS:OpenGL::OpenGL>,OpenGL::OpenGL,OpenGL::GL>")
else()
  set(OPENGL_TARGET "$<IF:$<BOOL:${OPENGL_opengl_LIBRARY}>,OpenGL::OpenGL,OpenGL::GL>")
endif()
