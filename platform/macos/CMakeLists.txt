######################################################################################
# macOS integration: launchd agent that loads the audio firmware at plug-in
######################################################################################
#
# Installed to share/libfreenect/macos; the user (or a package) copies it to
# ~/Library/LaunchAgents or /Library/LaunchAgents, see README.md.

set (FWLOADER_PATH "${CMAKE_INSTALL_PREFIX}/bin/freenect-fwloader")
configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/launchd/org.openkinect.fwloader.plist.in"
                "${CMAKE_CURRENT_BINARY_DIR}/org.openkinect.fwloader.plist" @ONLY)

if (BUILD_EXAMPLES)
  # The agent runs freenect-fwloader, which is built with the examples.
  install (FILES "${CMAKE_CURRENT_BINARY_DIR}/org.openkinect.fwloader.plist"
           DESTINATION "share/libfreenect/macos")
endif ()
install (FILES "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
         DESTINATION "share/doc/libfreenect/macos")

######################################################################################
# The Kinect's beamformed microphone (needs the audio DSP):
# - KinectBeamMic.driver: a Core Audio server plug-in (user space, loaded by
#   coreaudiod from /Library/Audio/Plug-Ins/HAL) with the microphone
#   "Kinect Beamformed Microphone" and a hidden sink that feeds it;
# - freenect-coreaudio-mic.app ("Kinect Beam Mic"): the background helper
#   that beamforms the Kinect's 4 microphones into that sink, started at login
#   by the launchd agent org.openkinect.beam-mic.plist.
# Both are signed ad hoc after the build (Apple Silicon needs a signature);
# for distribution, sign them with a Developer ID instead.
######################################################################################
if (BUILD_AUDIO_DSP)
  set (BEAM_MIC_DIR "share/libfreenect/macos")

  add_library (KinectBeamMic MODULE beam-mic/KinectBeamMic.c)
  set_target_properties (KinectBeamMic PROPERTIES
    BUNDLE TRUE
    BUNDLE_EXTENSION driver
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/beam-mic/KinectBeamMic-Info.plist"
    C_VISIBILITY_PRESET hidden)
  target_link_libraries (KinectBeamMic "-framework CoreAudio" "-framework CoreFoundation")
  add_custom_command (TARGET KinectBeamMic POST_BUILD
    COMMAND codesign --force --sign - "$<TARGET_BUNDLE_DIR:KinectBeamMic>"
    COMMENT "Signing KinectBeamMic.driver (ad hoc)")
  install (TARGETS KinectBeamMic LIBRARY DESTINATION "${BEAM_MIC_DIR}")

  add_executable (freenect-coreaudio-mic MACOSX_BUNDLE beam-mic/freenect-coreaudio-mic.c)
  set_target_properties (freenect-coreaudio-mic PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/beam-mic/helper-Info.plist")
  target_include_directories (freenect-coreaudio-mic PRIVATE "${PROJECT_SOURCE_DIR}/include")
  target_link_libraries (freenect-coreaudio-mic freenect_audio_dsp_static
    "-framework CoreAudio" "-framework CoreFoundation")
  add_custom_command (TARGET freenect-coreaudio-mic POST_BUILD
    COMMAND codesign --force --sign - "$<TARGET_BUNDLE_DIR:freenect-coreaudio-mic>"
    COMMENT "Signing freenect-coreaudio-mic.app (ad hoc)")
  install (TARGETS freenect-coreaudio-mic BUNDLE DESTINATION "${BEAM_MIC_DIR}")

  set (BEAM_MIC_HELPER_PATH
    "${CMAKE_INSTALL_PREFIX}/${BEAM_MIC_DIR}/freenect-coreaudio-mic.app/Contents/MacOS/freenect-coreaudio-mic")
  configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/launchd/org.openkinect.beam-mic.plist.in"
                  "${CMAKE_CURRENT_BINARY_DIR}/org.openkinect.beam-mic.plist" @ONLY)
  install (FILES "${CMAKE_CURRENT_BINARY_DIR}/org.openkinect.beam-mic.plist" DESTINATION "${BEAM_MIC_DIR}")
endif ()
