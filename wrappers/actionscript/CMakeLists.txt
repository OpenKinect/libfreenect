######################################################################################
# ActionScript socket server builder
######################################################################################

add_executable(as3-server server/as3-server.c)
if(APPLE)
	set(CMAKE_EXE_LINKER_FLAGS "-framework CoreFoundation -framework IOKit")
endif()
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../wrappers/c_sync)
target_link_libraries(as3-server freenect_sync_static m)
