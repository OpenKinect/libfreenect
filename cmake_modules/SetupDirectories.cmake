# Default installation directory, based on operating system
IF (PROJECT_OS_WIN)
    SET (CMAKE_INSTALL_PREFIX "C:\\Program Files\\libfreenect" CACHE PATH "Installation directory")
ELSE (PROJECT_OS_WIN)
    SET (CMAKE_INSTALL_PREFIX "/usr/local" CACHE PATH "Installation directory")
ENDIF (PROJECT_OS_WIN)

STRING (TOLOWER ${PROJECT_NAME} projectNameLower)
SET (PROJECT_INCLUDE_INSTALL_DIR "include/${projectNameLower}")
SET (PROJECT_MANPAGE_INSTALL_DIR "share/man")
SET (PROJECT_LIBRARY_INSTALL_DIR "lib")

MESSAGE (STATUS "${PROJECT_NAME} will be installed to ${CMAKE_INSTALL_PREFIX}")
MESSAGE (STATUS "Headers will be installed to ${CMAKE_INSTALL_PREFIX}/${PROJECT_INCLUDE_INSTALL_DIR}")
MESSAGE (STATUS "Libraries will be installed to ${CMAKE_INSTALL_PREFIX}/${PROJECT_LIBRARY_INSTALL_DIR}")
