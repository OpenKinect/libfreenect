include_directories (${CMAKE_SOURCE_DIR}/include)
include_directories (${CMAKE_SOURCE_DIR}/lib)

find_package(Threads REQUIRED)
find_package(USB REQUIRED)
include_directories(${USB_INCLUDE_DIRS})

SET(SRC motors.c accelerometers.c core.c cameras.c usb_libusb10.c)

if (NOT PROJECT_OS_WIN)
	add_library (freenect SHARED ${SRC} ${CMAKE_CURRENT_BINARY_DIR}/inits.c)
	set_target_properties ( freenect PROPERTIES
		VERSION ${PROJECT_VER}
		SOVERSION ${PROJECT_APIVER})	
	install (TARGETS freenect
		DESTINATION "${PROJECT_LIBRARY_INSTALL_DIR}")
endif (NOT PROJECT_OS_WIN)

add_library (freenectstatic STATIC ${SRC} ${CMAKE_CURRENT_BINARY_DIR}/inits.c)
set_target_properties (freenectstatic PROPERTIES
	OUTPUT_NAME freenect)
install (TARGETS freenectstatic
	DESTINATION "${PROJECT_LIBRARY_INSTALL_DIR}")

find_library (PTHREAD pthread)
target_link_libraries (freenect ${CMAKE_THREAD_LIBS_INIT} ${USB_LIBRARIES})
target_link_libraries (freenectstatic ${CMAKE_THREAD_LIBS_INIT} ${USB_LIBRARIES})


add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/inits.c
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/geninits.py
    MAIN_DEPENDENCY inits.txt
    COMMAND python ${CMAKE_CURRENT_SOURCE_DIR}/geninits.py ${CMAKE_CURRENT_SOURCE_DIR}/inits.txt ${CMAKE_CURRENT_BINARY_DIR}/inits.c)

# Install the header file
install (FILES "../include/libfreenect.h"
	DESTINATION ${PROJECT_INCLUDE_INSTALL_DIR})
