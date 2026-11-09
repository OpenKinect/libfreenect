######################################################################################
# libfreenect_nui: the optional skeleton tracker (BUILD_NUI, off by default)
#
# It needs Microsoft's models (DATABASE.WSDF, HEADPOSITION.MSHDF), which are
# never part of libfreenect: freenect-driver-install --fetch-nui-models or
# fwfetcher.py --nui extract them from Microsoft's Kinect Runtime download.
######################################################################################

set(NUI_SRC inflate.c models.c forest.c segment.c proposals.c fit.c output.c nui.c)

add_library(freenect_nui SHARED ${NUI_SRC})
add_library(freenect_nui_static STATIC ${NUI_SRC})
set_target_properties(freenect_nui PROPERTIES VERSION ${PROJECT_VER} SOVERSION ${PROJECT_APIVER})
if (MSVC)
  set_target_properties(freenect_nui_static PROPERTIES OUTPUT_NAME freenect_nui_s)
else ()
  set_target_properties(freenect_nui_static PROPERTIES OUTPUT_NAME freenect_nui POSITION_INDEPENDENT_CODE ON)
endif ()
foreach (t freenect_nui freenect_nui_static)
  target_include_directories(${t} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
  if (UNIX)
    target_link_libraries(${t} m)
  endif ()
endforeach ()
install(TARGETS freenect_nui freenect_nui_static DESTINATION "${PROJECT_LIBRARY_INSTALL_DIR}")
install(FILES "${CMAKE_SOURCE_DIR}/include/libfreenect_nui.h" DESTINATION ${PROJECT_INCLUDE_INSTALL_DIR})

if (BUILD_TESTS)
  # Needs the models; skipped (exit code 77) where they are not installed.
  add_executable(test_nui_forest test_nui_forest.c ${NUI_SRC})
  target_include_directories(test_nui_forest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/include)
  if (UNIX)
    target_link_libraries(test_nui_forest m)
  endif ()
  add_test(NAME nui_forest COMMAND test_nui_forest)
  set_tests_properties(nui_forest PROPERTIES SKIP_RETURN_CODE 77)
endif ()

if (BUILD_TESTS)
  # The whole tracker on a rendered scene: a person built from capsules on a
  # floor in front of a wall. Needs the models; skipped without them.
  add_executable(test_nui_tracker test_nui_tracker.c ${NUI_SRC})
  target_include_directories(test_nui_tracker PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/include)
  if (UNIX)
    target_link_libraries(test_nui_tracker m)
  endif ()
  add_test(NAME nui_tracker COMMAND test_nui_tracker)
  set_tests_properties(nui_tracker PROPERTIES SKIP_RETURN_CODE 77)
endif ()
