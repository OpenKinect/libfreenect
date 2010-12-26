######################################################################################
# ActionScript socket server builder
######################################################################################

if (WIN32)
  set_source_files_properties(server/as3-server.c PROPERTIES LANGUAGE CXX)

  set(THREADS_USE_PTHREADS_WIN32 true)
  find_package(Threads REQUIRED)

  include_directories(${THREADS_PTHREADS_INCLUDE_DIR})
endif()

add_executable(as3-server server/as3-server.c)
if(APPLE)
	set(CMAKE_EXE_LINKER_FLAGS "-framework CoreFoundation -framework IOKit")
endif()
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../wrappers/c_sync)

if (WIN32)
   set(MATH_LIB "")
else(WIN32)
  set(MATH_LIB "m")
endif()
  
target_link_libraries(as3-server freenect_sync_static ${MATH_LIB})
