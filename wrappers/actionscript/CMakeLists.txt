######################################################################################
# ActionScript socket server builder
######################################################################################

find_package(JPEG REQUIRED)

include_directories(${JPEG_INCLUDE_DIR})

add_library (as3_jpeg server/as3_jpeg.c)
target_link_libraries (as3_jpeg ${JPEG_LIBRARIES})

add_library (freenect_network server/freenect_network.c)

if (WIN32)
  set_source_files_properties(server/as3-server.c PROPERTIES LANGUAGE CXX)

  set(THREADS_USE_PTHREADS_WIN32 true)
  find_package(Threads REQUIRED)

  include_directories(${THREADS_PTHREADS_INCLUDE_DIR})
endif()

add_executable(as3-server server/as3-server.c)
if(APPLE)
	set(CMAKE_EXE_LINKER_FLAGS "-framework CoreFoundation -framework IOKit")
else(APPLE)
	find_package(Threads REQUIRED)
	if (WIN32)
	   set(MATH_LIB "")
	else(WIN32)
	  set(MATH_LIB "m")
	endif()

endif()
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../wrappers/c_sync)

target_link_libraries(as3-server freenect_sync_static as3_jpeg freenect_network ${MATH_LIB} ${CMAKE_THREAD_LIBS_INIT})
