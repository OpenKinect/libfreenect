######################################################################################
# Python extensions: freenect, freenect_audio, freenect_nui (Cython)
######################################################################################
#
# Two ways to build them:
#
# 1. As a Python package (pyproject.toml, scikit-build-core): from the
#    repository root,
#        uv pip install ./wrappers/python      (or pip install, or uv build)
#    builds libfreenect itself, statically, into the modules, so nothing else
#    needs to be installed. On Windows, point CMake at libusb, for example
#        uv pip install ./wrappers/python -C cmake.define.LIBUSB_1_INCLUDE_DIR=C:/libusb/include/libusb-1.0
#                                         -C cmake.define.LIBUSB_1_LIBRARY=C:/libusb/VS2022/MS64/dll/libusb-1.0.lib
#    and libusb-1.0.dll from next to that .lib goes into the package.
#
# 2. With libfreenect's own build: -DBUILD_PYTHON3=ON. The modules then link
#    libfreenect's shared libraries and install to Python's site-packages.

cmake_minimum_required(VERSION 3.16)

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(freenect_python C)
  set(FREENECT_PYTHON_STANDALONE ON)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
  option(FREENECT_PYTHON_NUI "Build freenect_nui (skeleton tracking)" ON)
  foreach (o BUILD_EXAMPLES BUILD_TESTS BUILD_FAKENECT BUILD_CPP BUILD_CV BUILD_PYTHON BUILD_PYTHON2 BUILD_PYTHON3
             BUILD_OPENNI2_DRIVER BUILD_DRIVER_INSTALLER INSTALL_LINUX_INTEGRATION BUILD_V4L2_BRIDGE BUILD_AS3_SERVER)
    set(${o} OFF CACHE BOOL "" FORCE)
  endforeach ()
  set(BUILD_C_SYNC ON CACHE BOOL "" FORCE)
  set(BUILD_AUDIO_DSP ON CACHE BOOL "" FORCE)
  set(BUILD_NUI ${FREENECT_PYTHON_NUI} CACHE BOOL "" FORCE)
  # libfreenect's own install rules are not part of the Python package
  # (pyproject.toml installs only the "python" component)
  add_subdirectory(../.. libfreenect EXCLUDE_FROM_ALL)
  set(FN_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
  set(FN_CORE freenectstatic)
  set(FN_SYNC freenect_sync_static)
  set(FN_DSP freenect_audio_dsp_static)
  set(FN_NUI freenect_nui_static)
  set(FN_HAVE_NUI ${FREENECT_PYTHON_NUI})
  if (DEFINED SKBUILD_PLATLIB_DIR)
    set(FN_PY_DEST ".")
  endif ()
else ()
  set(FREENECT_PYTHON_STANDALONE OFF)
  set(FN_ROOT "${PROJECT_SOURCE_DIR}")
  set(FN_CORE freenect)
  set(FN_SYNC freenect_sync)
  set(FN_DSP freenect_audio_dsp)
  set(FN_NUI freenect_nui)
  set(FN_HAVE_NUI ${BUILD_NUI})
  if (NOT BUILD_AUDIO_DSP)
    message(FATAL_ERROR "The Python extensions need BUILD_AUDIO_DSP")
  endif ()
endif ()

find_package(Python3 REQUIRED COMPONENTS Interpreter Development.Module NumPy)
if (NOT DEFINED FN_PY_DEST)
  set(FN_PY_DEST "${Python3_SITEARCH}")
endif ()
execute_process(COMMAND "${Python3_EXECUTABLE}" -m cython --version
                RESULT_VARIABLE _cython_missing OUTPUT_QUIET ERROR_QUIET)
if (_cython_missing)
  message(FATAL_ERROR "The Python extensions need Cython 3 (${Python3_EXECUTABLE} -m pip install cython)")
endif ()

# freenect_cython(module pyx libraries...)
function(freenect_cython name pyx)
  add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${name}.c"
    COMMAND "${Python3_EXECUTABLE}" -m cython -3 --fast-fail -o "${CMAKE_CURRENT_BINARY_DIR}/${name}.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/${pyx}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${pyx}"
    COMMENT "Cython ${pyx}")
  Python3_add_library(py_${name} MODULE WITH_SOABI "${CMAKE_CURRENT_BINARY_DIR}/${name}.c")
  set_target_properties(py_${name} PROPERTIES OUTPUT_NAME "${name}"
                        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/python3"
                        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/python3")
  target_include_directories(py_${name} PRIVATE "${FN_ROOT}/include" "${FN_ROOT}/wrappers/c_sync")
  target_compile_definitions(py_${name} PRIVATE NPY_NO_DEPRECATED_API=NPY_1_7_API_VERSION)
  target_link_libraries(py_${name} PRIVATE Python3::NumPy ${ARGN})
  install(TARGETS py_${name} LIBRARY DESTINATION "${FN_PY_DEST}" COMPONENT python
                             RUNTIME DESTINATION "${FN_PY_DEST}" COMPONENT python)
endfunction()

freenect_cython(freenect freenect.pyx ${FN_SYNC} ${FN_CORE})
freenect_cython(freenect_audio freenect_audio.pyx ${FN_DSP})
if (FN_HAVE_NUI)
  freenect_cython(freenect_nui freenect_nui.pyx ${FN_NUI})
endif ()

# Windows: the package carries libusb-1.0.dll (found next to the import
# library), which Python finds next to the modules.
if (WIN32 AND FREENECT_PYTHON_STANDALONE)
  set(FREENECT_PYTHON_LIBUSB_DLL "" CACHE FILEPATH "libusb-1.0.dll to put into the package (default: next to LIBUSB_1_LIBRARY)")
  set(_dll "${FREENECT_PYTHON_LIBUSB_DLL}")
  if (NOT _dll AND LIBUSB_1_LIBRARY)
    get_filename_component(_libdir "${LIBUSB_1_LIBRARY}" DIRECTORY)
    if (EXISTS "${_libdir}/libusb-1.0.dll")
      set(_dll "${_libdir}/libusb-1.0.dll")
    endif ()
  endif ()
  if (_dll)
    install(FILES "${_dll}" DESTINATION "${FN_PY_DEST}" COMPONENT python)
  else ()
    message(WARNING "libusb-1.0.dll not found next to ${LIBUSB_1_LIBRARY}; set FREENECT_PYTHON_LIBUSB_DLL, "
                    "or put the DLL on the PATH yourself")
  endif ()
endif ()
