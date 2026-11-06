######################################################################################
# freenect-driver-install: binds the Kinect v1 to Microsoft's in-box WinUSB driver
######################################################################################

add_executable(freenect-driver-install freenect-driver-install.c)
target_compile_definitions(freenect-driver-install PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
target_link_libraries(freenect-driver-install setupapi cfgmgr32 newdev crypt32 wintrust cryptui shell32 ole32 advapi32)
if (MSVC)
  # Request elevation only when needed: --list works for normal users.
  target_link_options(freenect-driver-install PRIVATE "/MANIFESTUAC:level='asInvoker'")
endif()
install(TARGETS freenect-driver-install DESTINATION bin)
