#include <string>
#include "ColorStream.hpp"

using namespace FreenectDriver;


ColorStream::ColorStream(Freenect::FreenectDevice* pDevice) : VideoStream(pDevice)
{
  video_mode = makeOniVideoMode(ONI_PIXEL_FORMAT_RGB888, 640, 480, 30);
  setVideoMode(video_mode);
  pDevice->startVideo();
}

// Add video modes here as you implement them
ColorStream::FreenectVideoModeMap ColorStream::getSupportedVideoModes()
{
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

OniStatus ColorStream::setVideoMode(OniVideoMode requested_mode)
{
  FreenectVideoModeMap supported_video_modes = getSupportedVideoModes();
  FreenectVideoModeMap::const_iterator matched_mode_iter = supported_video_modes.find(requested_mode);
  if (matched_mode_iter == supported_video_modes.end())
    return ONI_STATUS_NOT_SUPPORTED;

  freenect_video_format format = matched_mode_iter->second.first;
  freenect_resolution resolution = matched_mode_iter->second.second;

  try { device->setVideoFormat(format, resolution); }
  catch (std::runtime_error e)
  {
    LogError("Format " + to_string(format) + " and resolution " + to_string(resolution) + " combination not supported by libfreenect");
    return ONI_STATUS_NOT_SUPPORTED;
  }
  video_mode = requested_mode;
  return ONI_STATUS_OK;
}

void ColorStream::populateFrame(void* data, OniFrame* frame) const
{
  frame->sensorType = SENSOR_TYPE;
  frame->stride = video_mode.resolutionX * 3;
  frame->cropOriginX = 0;
  frame->cropOriginY = 0;
  frame->croppingEnabled = false;

  // copy stream buffer from freenect
  switch (video_mode.pixelFormat)
  {
    default:
      LogError("Pixel format " + to_string(video_mode.pixelFormat) + " not supported by populateFrame()");
      return;

    case ONI_PIXEL_FORMAT_RGB888:
      uint8_t* source = static_cast<uint8_t*>(data);
      uint8_t* target = static_cast<uint8_t*>(frame->data);
      std::copy(source, source + frame->dataSize, target);
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
