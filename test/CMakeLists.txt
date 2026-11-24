######################################################################################
# Unit tests that need no Kinect. Run them with: ctest --test-dir build -C Release
######################################################################################

include_directories(${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/include)

# Hotplug bookkeeping (src/hotplug.c)
add_executable(test_hotplug test_hotplug.c ${PROJECT_SOURCE_DIR}/src/hotplug.c)
add_test(NAME hotplug COMMAND test_hotplug)

# The hotplug path of src/core.c against a simulated USB layer (the test
# provides the fnusb_* and camera functions that core.c calls)
add_executable(test_core_hotplug test_core_hotplug.c ${PROJECT_SOURCE_DIR}/src/core.c ${PROJECT_SOURCE_DIR}/src/hotplug.c)
add_test(NAME core_hotplug COMMAND test_core_hotplug)

# freenect-v4l2's pixel conversions (examples/v4l2_convert.h)
add_executable(test_v4l2_convert test_v4l2_convert.c)
target_include_directories(test_v4l2_convert PRIVATE ${PROJECT_SOURCE_DIR}/examples)
add_test(NAME v4l2_convert COMMAND test_v4l2_convert)

# Isochronous stream health check and VM detection (src/iso_diag.c)
add_executable(test_iso_diag test_iso_diag.c ${PROJECT_SOURCE_DIR}/src/iso_diag.c)
add_test(NAME iso_diag COMMAND test_iso_diag)

# The viewers' GLFW window layer (examples/fn_glut.c): a close request reaches
# the viewer as Esc, and a viewer that is stuck is ended by the watchdog.
# Needs a display (CI uses xvfb-run); skipped without one.
if (BUILD_EXAMPLES AND FREENECT_GL_BACKEND STREQUAL "GLFW" AND UNIX AND NOT APPLE)
  add_executable(test_fn_glut test_fn_glut.c ${PROJECT_SOURCE_DIR}/examples/fn_glut.c)
  target_compile_definitions(test_fn_glut PRIVATE FN_USE_GLFW)
  target_include_directories(test_fn_glut PRIVATE ${PROJECT_SOURCE_DIR}/examples)
  target_link_libraries(test_fn_glut glfw "${OPENGL_TARGET}")
  add_test(NAME fn_glut_close COMMAND test_fn_glut normal)
  # killed by SIGALRM: the shell reports 128 + 14
  add_test(NAME fn_glut_watchdog
           COMMAND sh -c "\"$<TARGET_FILE:test_fn_glut>\" blocked; c=$?; [ $c -eq 77 ] && exit 77; [ $c -eq 142 ]")
  set_tests_properties(fn_glut_close fn_glut_watchdog PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 20)
endif ()

# Frame reassembly from the camera's packets (src/stream.c): only complete
# frames are delivered, and the first video frames after a start are skipped.
add_executable(test_stream test_stream.c ${PROJECT_SOURCE_DIR}/src/stream.c)
add_test(NAME stream COMMAND test_stream)

# 320x240 and 80x60 depth (src/depth_decimate.h): the same pixels as
# Microsoft's runtime, mirrored and unmirrored.
add_executable(test_decimate test_decimate.c)
add_test(NAME decimate COMMAND test_decimate)

# Raw depth -> mm (src/depth_lut.h) against Microsoft's runtime (§20.2), in
# the three ranges.
add_executable(test_depth_lut test_depth_lut.c)
add_test(NAME depth_lut COMMAND test_depth_lut)

# Kinect for Windows colour settings (src/color_settings.h): the register
# encodings of the reference's §10C.2.
add_executable(test_color_settings test_color_settings.c)
add_test(NAME color_settings COMMAND test_color_settings)

# The colour settings' device side (src/color.c) against a simulated camera:
# order of writes, read-back, preconditions, when they are sent.
add_executable(test_color_device test_color_device.c ${PROJECT_SOURCE_DIR}/src/color.c)
add_test(NAME color_device COMMAND test_color_device)

# Near mode and IR brightness (src/flags.c) share camera register 0x15:
# against a simulated camera on the GM command channel.
add_executable(test_near_ir test_near_ir.c ${PROJECT_SOURCE_DIR}/src/flags.c)
add_test(NAME near_ir COMMAND test_near_ir)

# Depth/colour phase alignment (src/stream_timing.h): the driver's nudge
# value and Kinect10's phase estimate.
add_executable(test_stream_timing test_stream_timing.c)
add_test(NAME stream_timing COMMAND test_stream_timing)

# The coordinate mapper (src/coordinate_mapper.c) against Microsoft's runtime
# (§20.5): vectors from the reference's tools/kinect10_depth_ref.py, to the bit.
add_executable(test_coordinate_mapper test_coordinate_mapper.c)
if (NOT MSVC)
  target_compile_options(test_coordinate_mapper PRIVATE -ffp-contract=off)
endif ()
if (UNIX)
  target_link_libraries(test_coordinate_mapper m)
endif ()
add_test(NAME coordinate_mapper COMMAND test_coordinate_mapper)

# The microphone array descriptor of the audio firmware (src/mic_geometry.h, §40.5.3)
add_executable(test_mic_geometry test_mic_geometry.c)
add_test(NAME mic_geometry COMMAND test_mic_geometry)

# The audio DSP (src/mclt.c, src/audio_dsp.c, §40.6): the filterbank against
# the reference's tools/mclt_ref.py, the designed beams, direction finding on
# simulated sources, and the weights file.
if (BUILD_AUDIO_DSP)
  add_executable(test_audio_dsp test_audio_dsp.c ${PROJECT_SOURCE_DIR}/src/mclt.c)
  if (WITH_SPEEXDSP AND SPEEXDSP_INCLUDE_DIR AND SPEEXDSP_LIBRARY)
    target_compile_definitions(test_audio_dsp PRIVATE HAVE_SPEEXDSP)
    target_include_directories(test_audio_dsp PRIVATE ${SPEEXDSP_INCLUDE_DIR})
    target_link_libraries(test_audio_dsp ${SPEEXDSP_LIBRARY})
  endif ()
  if (UNIX)
    target_link_libraries(test_audio_dsp m)
  endif ()
  add_test(NAME audio_dsp COMMAND test_audio_dsp)
endif ()
