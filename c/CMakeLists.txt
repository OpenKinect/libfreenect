PROJECT(libfreenect)
set (PROJECT_VER_MAJOR 0)
set (PROJECT_VER_MINOR 0)
set (PROJECT_VER_PATCH 0)
set (PROJECT_VER 
	"${PROJECT_VER_MAJOR}.${PROJECT_VER_MINOR}.${PROJECT_VER_PATCH}")
set (PROJECT_APIVER 
	"${PROJECT_VER_MAJOR}.${PROJECT_VER_MINOR}")

cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/Modules/")

set(CMAKE_C_FLAGS "-Wall -O3 -g")

# Find the host operating system and architecture
include (FindOS)
# Set up installation directories
include (SetupDirectories)

# Add source directories
add_subdirectory (lib)
add_subdirectory (examples)

# Create an uninstall target
configure_file(
    "${CMAKE_SOURCE_DIR}/Modules/UninstallTarget.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/UninstallTarget.cmake"
    IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/UninstallTarget.cmake)
