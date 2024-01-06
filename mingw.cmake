# CMake toolchain file, cf. README.mingw_cross

# the name of the target operating system
set(CMAKE_SYSTEM_NAME Windows)

IF("${GNU_HOST}" STREQUAL "")
	SET(GNU_HOST x86_64-w64-mingw32)
ENDIF()

# which compilers to use for C and C++
set(CMAKE_C_COMPILER   ${GNU_HOST}-gcc)
set(CMAKE_CXX_COMPILER ${GNU_HOST}-g++)

# where is the target environment located
set(CMAKE_FIND_ROOT_PATH /usr/${GNU_HOST})

# adjust the default behavior of the FIND_XXX() commands:
# do not search programs in the host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# search headers and libraries only in the target environment
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
