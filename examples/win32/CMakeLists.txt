ADD_EXECUTABLE(freenect_min_demo Freenect-MinimalSample.cpp)
ADD_EXECUTABLE(glview glview.c)

INCLUDE_DIRECTORIES(${PTHREADS_INCLUDE_DIR})

TARGET_LINK_LIBRARIES(freenect_min_demo libfreenectstatic)
TARGET_LINK_LIBRARIES(glview libfreenectstatic ${PTHREADS_LIBRARY} OpenGL32 GLU32 GLUT32)
