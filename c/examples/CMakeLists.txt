option (BUILD_EXAMPLES "Build example programs" ON)

if (BUILD_EXAMPLES)
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
	if(PROJECT_OS_OSX)
		set(CMAKE_EXE_LINKER_FLAGS "-framework OpenGL -framework GLUT")
		target_link_libraries(glview freenect)
	else()
		target_link_libraries(glview freenect GL GLU glut m)
	endif()

	install (TARGETS glview
		DESTINATION bin)

endif (BUILD_EXAMPLES)
