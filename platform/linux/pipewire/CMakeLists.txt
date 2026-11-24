######################################################################################
# PipeWire: the Kinect's microphone array as one beamformed microphone
######################################################################################
#
# - freenect_beam.so: libfreenect's beamformer as a LADSPA plugin (no LADSPA
#   or PipeWire headers needed);
# - kinect-mic.conf + 51-freenect-kinect-mic.conf: WirePlumber 0.5 loads the
#   plugin in a filter graph when the Kinect's microphones appear;
# - freenect-pipewire-mic (+ a systemd user unit): the same as a small
#   PipeWire client, for WirePlumber 0.4; needs libpipewire-0.3.

set (FREENECT_LADSPA_DIR "${PROJECT_LIBRARY_INSTALL_DIR}/ladspa" CACHE STRING "Where to install the freenect_beam LADSPA plugin")
set (FREENECT_PIPEWIRE_DATADIR "share/libfreenect/pipewire" CACHE STRING "Where to install kinect-mic.conf")
set (FREENECT_WIREPLUMBER_CONF_DIR "share/wireplumber/wireplumber.conf.d" CACHE STRING "Where to install the WirePlumber rules")
set (FREENECT_SYSTEMD_USER_UNIT_DIR "lib/systemd/user" CACHE STRING "Where to install freenect-pipewire-mic.service")

foreach (v FREENECT_LADSPA_DIR FREENECT_PIPEWIRE_DATADIR)
  if (IS_ABSOLUTE "${${v}}")
    set (${v}_ABS "${${v}}")
  else ()
    set (${v}_ABS "${CMAKE_INSTALL_PREFIX}/${${v}}")
  endif ()
endforeach ()

# The LADSPA plugin: only the DSP library, statically, so that PipeWire
# loads no other libfreenect library into its process.
add_library (freenect_beam_ladspa MODULE freenect-beam-ladspa.c)
set_target_properties (freenect_beam_ladspa PROPERTIES PREFIX "" OUTPUT_NAME "freenect_beam" C_VISIBILITY_PRESET hidden)
target_include_directories (freenect_beam_ladspa PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_link_libraries (freenect_beam_ladspa freenect_audio_dsp_static m)
if (NOT APPLE)
  # export only ladspa_descriptor, not the DSP library it carries
  target_link_options (freenect_beam_ladspa PRIVATE "-Wl,--exclude-libs,ALL")
endif ()
install (TARGETS freenect_beam_ladspa DESTINATION "${FREENECT_LADSPA_DIR}")

set (FREENECT_LADSPA_PLUGIN "${FREENECT_LADSPA_DIR_ABS}/freenect_beam.so")
configure_file (kinect-mic.conf.in "${CMAKE_CURRENT_BINARY_DIR}/kinect-mic.conf" @ONLY)
set (FREENECT_PIPEWIRE_DATADIR "${FREENECT_PIPEWIRE_DATADIR_ABS}")
configure_file (51-freenect-kinect-mic.conf.in "${CMAKE_CURRENT_BINARY_DIR}/51-freenect-kinect-mic.conf" @ONLY)
install (FILES "${CMAKE_CURRENT_BINARY_DIR}/kinect-mic.conf" DESTINATION "${FREENECT_PIPEWIRE_DATADIR}")
install (FILES "${CMAKE_CURRENT_BINARY_DIR}/51-freenect-kinect-mic.conf" DESTINATION "${FREENECT_WIREPLUMBER_CONF_DIR}")

find_package (PkgConfig QUIET)
if (PKG_CONFIG_FOUND)
  pkg_check_modules (PIPEWIRE QUIET IMPORTED_TARGET libpipewire-0.3)
endif ()
if (PIPEWIRE_FOUND)
  message (STATUS "freenect-pipewire-mic: with PipeWire ${PIPEWIRE_VERSION}")
  add_executable (freenect-pipewire-mic freenect-pipewire-mic.c)
  target_include_directories (freenect-pipewire-mic PRIVATE "${PROJECT_SOURCE_DIR}/include")
  target_link_libraries (freenect-pipewire-mic freenect_audio_dsp PkgConfig::PIPEWIRE m)
  install (TARGETS freenect-pipewire-mic DESTINATION bin)
  set (FREENECT_PIPEWIRE_MIC_PATH "${CMAKE_INSTALL_PREFIX}/bin/freenect-pipewire-mic")
  configure_file (freenect-pipewire-mic.service.in "${CMAKE_CURRENT_BINARY_DIR}/freenect-pipewire-mic.service" @ONLY)
  install (FILES "${CMAKE_CURRENT_BINARY_DIR}/freenect-pipewire-mic.service" DESTINATION "${FREENECT_SYSTEMD_USER_UNIT_DIR}")
else ()
  message (STATUS "freenect-pipewire-mic: not built (no libpipewire-0.3 development files)")
endif ()
