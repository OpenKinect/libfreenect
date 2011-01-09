# Finds OpenCV library
#
#  OpenCV_INCLUDE_DIR - where to find cv.h, etc.
#  OpenCV_LIBRARIES   - List of libraries when using OpenCV.
#  OpenCV_FOUND       - True if OpenCV found.
#

find_path(OpenCV_INCLUDE_DIR opencv/cv.h
  /opt/local/include
  /usr/local/include
  /usr/include
)

find_library(OpenCV_Core_LIBRARY
  NAMES opencv_core
  PATHS /usr/lib /usr/local/lib /opt/local/lib
)

find_library(OpenCV_Highgui_LIBRARY
  NAMES opencv_highgui
  PATHS /usr/lib /usr/local/lib /opt/local/lib
)

find_library(OpenCV_Imgproc_LIBRARY
  NAMES opencv_imgproc
  PATHS /usr/lib /usr/local/lib /opt/local/lib
)

if (OpenCV_INCLUDE_DIR AND OpenCV_Core_LIBRARY AND OpenCV_Highgui_LIBRARY AND OpenCV_Imgproc_LIBRARY)
   set(OpenCV_FOUND TRUE)
   set( OpenCV_LIBRARIES ${OpenCV_Core_LIBRARY} ${OpenCV_Highgui_LIBRARY} ${OpenCV_Imgproc_LIBRARY})
else (OpenCV_INCLUDE_DIR AND OpenCV_Core_LIBRARY AND OpenCV_Highgui_LIBRARY AND OpenCV_Imgproc_LIBRARY)
   set(OpenCV_FOUND FALSE)
   set(OpenCV_LIBRARIES)
endif (OpenCV_INCLUDE_DIR AND OpenCV_Core_LIBRARY AND OpenCV_Highgui_LIBRARY AND OpenCV_Imgproc_LIBRARY)

if (OpenCV_FOUND)
   if (NOT OpenCV_FIND_QUIETLY)
      message(STATUS "Found OpenCV:")
	  message(STATUS " - Includes: ${OpenCV_INCLUDE_DIR}")
	  message(STATUS " - Libraries: ${OpenCV_LIBRARIES}")
   endif (NOT OpenCV_FIND_QUIETLY)
else (OpenCV_FOUND)
   if (OpenCV_FIND_REQUIRED)
      message(STATUS "Looked for OpenCV libraries named ${OpenCV_NAMES}.")
      message(STATUS "Include file detected: [${OpenCV_INCLUDE_DIR}].")
      message(STATUS "Lib file detected: [${OpenCV_LIBRARIES}].")
      message(FATAL_ERROR "=========> Could NOT find OpenCV library")
   endif (OpenCV_FIND_REQUIRED)
endif (OpenCV_FOUND)

mark_as_advanced(
  OpenCV_Core_LIBRARY
  OpenCV_INCLUDE_DIR
  )
