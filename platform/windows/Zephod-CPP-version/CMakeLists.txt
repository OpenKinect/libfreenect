cmake_minimum_required(VERSION 2.6)

PROJECT(libfreenect_zephod)

IF(NOT WIN32)
   MESSAGE(FATAL_ERROR "libfreenect_win is not currently supported on this platform")
ENDIF()

#Glob all source files
SET(CMAKE_MODULE_PATH ../../../cmake_modules)

FILE(GLOB ZEPHOD_FREENECT_SRCS "*.cpp")
FILE(GLOB ZEPHOD_FREENECT_HDRS "*.h")

FIND_PATH(USB_INCLUDE_DIR 
NAMES usb.h
PATHS libusb/include)
FIND_LIBRARY(USB_LIBRARY NAMES libusb.lib 
PATHS libusb/lib)

INCLUDE_DIRECTORIES(${USB_INCLUDE_DIR})

ADD_LIBRARY(libfreenect SHARED ${ZEPHOD_FREENECT_SRCS})

TARGET_LINK_LIBRARIES(libfreenect ${USB_LIBRARY})
