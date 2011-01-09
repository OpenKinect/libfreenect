# Copyright (c) 2009, Whispersoft s.r.l.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
# * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
# * Neither the name of Whispersoft s.r.l. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

# Finds LibJpeg library
#
#  LibJpeg_INCLUDE_DIR - where to find jpeglib.h, etc.
#  LibJpeg_LIBRARIES   - List of libraries when using LibJpeg.
#  LibJpeg_FOUND       - True if LibJpeg found.
#

if (LibJpeg_INCLUDE_DIR)
  # Already in cache, be silent
  set(LibJpeg_FIND_QUIETLY TRUE)
endif (LibJpeg_INCLUDE_DIR)

find_path(LibJpeg_INCLUDE_DIR jpeglib.h
  /opt/local/include
  /usr/local/include
  /usr/include
)

set(LibJpeg_NAMES jpeg)
find_library(LibJpeg_LIBRARY
  NAMES ${LibJpeg_NAMES}
  PATHS /usr/lib /usr/local/lib /opt/local/lib
)

if (LibJpeg_INCLUDE_DIR AND LibJpeg_LIBRARY)
   set(LibJpeg_FOUND TRUE)
   set( LibJpeg_LIBRARIES ${LibJpeg_LIBRARY} )
else (LibJpeg_INCLUDE_DIR AND LibJpeg_LIBRARY)
   set(LibJpeg_FOUND FALSE)
   set(LibJpeg_LIBRARIES)
endif (LibJpeg_INCLUDE_DIR AND LibJpeg_LIBRARY)

if (LibJpeg_FOUND)
   if (NOT LibJpeg_FIND_QUIETLY)
      message(STATUS "Found LibJpeg: ${LibJpeg_LIBRARY}")
   endif (NOT LibJpeg_FIND_QUIETLY)
else (LibJpeg_FOUND)
   if (LibJpeg_FIND_REQUIRED)
      message(STATUS "Looked for LibJpeg libraries named ${LibJpeg_NAMES}.")
      message(STATUS "Include file detected: [${LibJpeg_INCLUDE_DIR}].")
      message(STATUS "Lib file detected: [${LibJpeg_LIBRARY}].")
      message(FATAL_ERROR "=========> Could NOT find LibJpeg library")
   endif (LibJpeg_FIND_REQUIRED)
endif (LibJpeg_FOUND)

mark_as_advanced(
  LibJpeg_LIBRARY
  LibJpeg_INCLUDE_DIR
  )
