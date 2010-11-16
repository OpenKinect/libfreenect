include_directories (${CMAKE_SOURCE_DIR}/include)
link_directories (${CMAKE_BINARY_DIR}/lib)

find_package(Threads REQUIRED)
find_package(OpenGL REQUIRED)
find_package(GLU REQUIRED)
find_package(GLUT REQUIRED)

find_package(USB REQUIRED)
include_directories(${USB_INCLUDE_DIRS})

add_executable(glview glview.c)
find_library (PTHREAD pthread)
IF(APPLE)
  set(CMAKE_EXE_LINKER_FLAGS "-framework OpenGL -framework GLUT")
  target_link_libraries(glview freenect)
ELSE()
  target_link_libraries(glview freenect GL GLU glut m)
ENDIF()
