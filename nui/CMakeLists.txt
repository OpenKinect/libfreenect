######################################################################################
# libfreenect_nui: the optional skeleton tracker (BUILD_NUI, off by default)
#
# It needs Microsoft's models (DATABASE.WSDF, HEADPOSITION.MSHDF), which are
# never part of libfreenect: freenect-driver-install --fetch-nui-models or
# fwfetcher.py --nui extract them from Microsoft's Kinect Runtime download.
######################################################################################

set(NUI_SRC inflate.c models.c forest.c segment.c proposals.c fit.c output.c hands.c gestures.c headforest.c pool.c nui.c)

# the forest runs on a thread pool (pool.c)
find_package(Threads REQUIRED)
link_libraries(${CMAKE_THREAD_LIBS_INIT})

add_library(freenect_nui SHARED ${NUI_SRC})
add_library(freenect_nui_static STATIC ${NUI_SRC})
set_target_properties(freenect_nui PROPERTIES VERSION ${PROJECT_VER} SOVERSION ${PROJECT_APIVER})
if (MSVC)
  set_target_properties(freenect_nui_static PROPERTIES OUTPUT_NAME freenect_nui_s)
else ()
  set_target_properties(freenect_nui_static PROPERTIES OUTPUT_NAME freenect_nui POSITION_INDEPENDENT_CODE ON)
endif ()
foreach (t freenect_nui freenect_nui_static)
  target_include_directories(${t} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)
  if (UNIX)
    target_link_libraries(${t} m)
  endif ()
endforeach ()
install(TARGETS freenect_nui freenect_nui_static DESTINATION "${PROJECT_LIBRARY_INSTALL_DIR}")
install(FILES "${PROJECT_SOURCE_DIR}/include/libfreenect_nui.h" DESTINATION ${PROJECT_INCLUDE_INSTALL_DIR})

if (BUILD_TESTS)
  # Needs the models; skipped (exit code 77) where they are not installed.
  add_executable(test_nui_forest test_nui_forest.c ${NUI_SRC})
  target_include_directories(test_nui_forest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/include)
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
  target_include_directories(test_nui_tracker PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/include)
  if (UNIX)
    target_link_libraries(test_nui_tracker m)
  endif ()
  add_test(NAME nui_tracker COMMAND test_nui_tracker)
  set_tests_properties(nui_tracker PROPERTIES SKIP_RETURN_CODE 77)
endif ()

if (BUILD_TESTS)
  # Hands and fingers on rendered depth; needs no model.
  add_executable(test_nui_hands test_nui_hands.c hands.c gestures.c segment.c)
  target_include_directories(test_nui_hands PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/include)
  if (UNIX)
    target_link_libraries(test_nui_hands m)
  endif ()
  add_test(NAME nui_hands COMMAND test_nui_hands)
endif ()

if (BUILD_TESTS)
  # Two-hand manipulation (turn, zoom, move) on synthetic hand paths; needs no model.
  add_executable(test_nui_gestures test_nui_gestures.c gestures.c segment.c)
  target_include_directories(test_nui_gestures PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/include)
  if (UNIX)
    target_link_libraries(test_nui_gestures m)
  endif ()
  add_test(NAME nui_gestures COMMAND test_nui_gestures)
endif ()

if (BUILD_TESTS)
  # The seated head detector with Microsoft's head forest; skipped without it.
  add_executable(test_nui_heads test_nui_heads.c headforest.c segment.c models.c inflate.c)
  target_include_directories(test_nui_heads PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/include)
  if (UNIX)
    target_link_libraries(test_nui_heads m)
  endif ()
  add_test(NAME nui_heads COMMAND test_nui_heads)
  set_tests_properties(nui_heads PROPERTIES SKIP_RETURN_CODE 77)
endif ()

if (BUILD_TESTS)
  # The joint filters (Holt, One-Euro, Kalman); needs no model.
  add_executable(test_nui_smooth test_nui_smooth.c)
  target_include_directories(test_nui_smooth PRIVATE ${PROJECT_SOURCE_DIR}/include)
  target_link_libraries(test_nui_smooth freenect_nui_static)
  if (UNIX)
    target_link_libraries(test_nui_smooth m)
  endif ()
  add_test(NAME nui_smooth COMMAND test_nui_smooth)
endif ()

if (BUILD_TESTS)
  # The skeleton fit's joint limits; needs no model.
  add_executable(test_nui_fit test_nui_fit.c fit.c segment.c)
  target_include_directories(test_nui_fit PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/include)
  if (UNIX)
    target_link_libraries(test_nui_fit m)
  endif ()
  add_test(NAME nui_fit COMMAND test_nui_fit)
endif ()
