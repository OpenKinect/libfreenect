#include "ColorStream.hpp"

using namespace FreenectDriver;


ColorStream::ColorStream(Freenect::FreenectDevice* pDevice) : VideoStream(pDevice) {
  video_mode = makeOniVideoMode(ONI_PIXEL_FORMAT_RGB888, 640, 480, 30);
  setVideoMode(video_mode);
}

// Add video modes here as you implement them
ColorStream::FreenectVideoModeMap ColorStream::getSupportedVideoModes() {
  FreenectVideoModeMap modes;
  //                    pixelFormat, resolutionX, resolutionY, fps    freenect_video_format, freenect_resolution
  modes[makeOniVideoMode(ONI_PIXEL_FORMAT_RGB888, 640, 480, 30)] = std::pair<freenect_video_format, freenect_resolution>(FREENECT_VIDEO_RGB, FREENECT_RESOLUTION_MEDIUM);


  return modes;

  /* working format possiblities
  FREENECT_VIDEO_RGB
  FREENECT_VIDEO_YUV_RGB
  FREENECT_VIDEO_YUV_RAW
  */
}

OniStatus ColorStream::setVideoMode(OniVideoMode requested_mode) {
  FreenectVideoModeMap supported_video_modes = getSupportedVideoModes();
  FreenectVideoModeMap::const_iterator matched_mode_iter = supported_video_modes.find(requested_mode);
  if (matched_mode_iter == supported_video_modes.end())
    return ONI_STATUS_NOT_SUPPORTED;

  freenect_video_format format = matched_mode_iter->second.first;
  freenect_resolution resolution = matched_mode_iter->second.second;

  try { device->setVideoFormat(format, resolution); }
  catch (std::runtime_error e) {
    printf("format-resolution combination not supported by libfreenect: %d-%d\n", format, resolution);
    return ONI_STATUS_NOT_SUPPORTED;
  }
  video_mode = requested_mode;
  return ONI_STATUS_OK;
}

void ColorStream::populateFrame(void* data, OniFrame* frame) const {
  frame->sensorType = sensor_type;
  frame->stride = video_mode.resolutionX*3;
  frame->cropOriginX = frame->cropOriginY = 0;
  frame->croppingEnabled = FALSE;

  // copy stream buffer from freenect
  switch (video_mode.pixelFormat) {
    default:
      printf("pixelFormat %d not supported by populateFrame\n", video_mode.pixelFormat);
      return;

    case ONI_PIXEL_FORMAT_RGB888:
      unsigned char* data_ptr = static_cast<unsigned char*>(data);
      unsigned char* frame_data = static_cast<unsigned char*>(frame->data);
      if (mirroring) {
        for (int i = 0; i < frame->dataSize; i += 3) {
          // find corresponding mirrored pixel
          unsigned int pixel = i / 3;
          unsigned int row = pixel / video_mode.resolutionX;
          unsigned int col = video_mode.resolutionX - (pixel % video_mode.resolutionX);
          unsigned int target = 3 * (row * video_mode.resolutionX + col);
          // copy it to this pixel
          frame_data[i] = data_ptr[target];
          frame_data[i+1] = data_ptr[target+1];
          frame_data[i+2] = data_ptr[target+2];
        }
      }
      else
        std::copy(data_ptr, data_ptr+frame->dataSize, frame_data);

      return;
  }
}

/* color video modes reference

FREENECT_VIDEO_RGB             = 0, //< Decompressed RGB mode (demosaicing done by libfreenect)
FREENECT_VIDEO_BAYER           = 1, //< Bayer compressed mode (raw information from camera)
FREENECT_VIDEO_YUV_RGB         = 5, //< YUV RGB mode
FREENECT_VIDEO_YUV_RAW         = 6, //< YUV Raw mode

ONI_PIXEL_FORMAT_RGB888 = 200,
ONI_PIXEL_FORMAT_YUV422 = 201,
ONI_PIXEL_FORMAT_JPEG = 204,
*/
