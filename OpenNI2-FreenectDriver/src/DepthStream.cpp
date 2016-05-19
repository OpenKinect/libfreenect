#include <string>
#include "DepthStream.hpp"

using namespace FreenectDriver;


DepthStream::DepthStream(Freenect::FreenectDevice* pDevice) : VideoStream(pDevice)
{
  video_mode = makeOniVideoMode(ONI_PIXEL_FORMAT_DEPTH_1_MM, 640, 480, 30);
  image_registration_mode = ONI_IMAGE_REGISTRATION_OFF;
  setVideoMode(video_mode);
  pDevice->startDepth();
}

// Add video modes here as you implement them
// Note: if image_registration_mode == ONI_IMAGE_REGISTRATION_DEPTH_TO_COLOR,
// setVideoFormat() will try FREENECT_DEPTH_REGISTERED first then fall back on what is set here.
DepthStream::FreenectDepthModeMap DepthStream::getSupportedVideoModes()
{
  FreenectDepthModeMap modes;
  //                      pixelFormat, resolutionX, resolutionY, fps
  modes[makeOniVideoMode(ONI_PIXEL_FORMAT_DEPTH_1_MM, 640, 480, 30)] = std::pair<freenect_depth_format, freenect_resolution>(FREENECT_DEPTH_MM, FREENECT_RESOLUTION_MEDIUM);


  return modes;
}

OniStatus DepthStream::setVideoMode(OniVideoMode requested_mode)
{
  FreenectDepthModeMap supported_video_modes = getSupportedVideoModes();
  FreenectDepthModeMap::const_iterator matched_mode_iter = supported_video_modes.find(requested_mode);
  if (matched_mode_iter == supported_video_modes.end())
    return ONI_STATUS_NOT_SUPPORTED;

  freenect_depth_format format = matched_mode_iter->second.first;
  freenect_resolution resolution = matched_mode_iter->second.second;
  if (image_registration_mode == ONI_IMAGE_REGISTRATION_DEPTH_TO_COLOR) // try forcing registration mode
    format = FREENECT_DEPTH_REGISTERED;

  try { device->setDepthFormat(format, resolution); }
  catch (std::runtime_error e)
  {
    LogError("Format " + to_string(format) + " and resolution " + to_string(resolution) + " combination not supported by libfreenect");
    if (image_registration_mode == ONI_IMAGE_REGISTRATION_DEPTH_TO_COLOR)
    {
      LogError("Could not enable image registration format; falling back to format defined in getSupportedVideoModes()");
      image_registration_mode = ONI_IMAGE_REGISTRATION_OFF;
      return setVideoMode(requested_mode);
    }
    return ONI_STATUS_NOT_SUPPORTED;
  }
  video_mode = requested_mode;
  return ONI_STATUS_OK;
}

void DepthStream::populateFrame(void* data, OniFrame* frame) const
{
  frame->sensorType = SENSOR_TYPE;
  frame->stride = video_mode.resolutionX * sizeof(uint16_t);

  if (cropping.enabled)
  {
    frame->height = cropping.height;
    frame->width = cropping.width;
    frame->cropOriginX = cropping.originX;
    frame->cropOriginY = cropping.originY;
    frame->croppingEnabled = true;
  }
  else
  {
    frame->cropOriginX = 0;
    frame->cropOriginY = 0;
    frame->croppingEnabled = false;
  }


  // copy stream buffer from freenect

  uint16_t* source = static_cast<uint16_t*>(data) + frame->cropOriginX + frame->cropOriginY * video_mode.resolutionX;
  uint16_t* target = static_cast<uint16_t*>(frame->data);
  const unsigned int skipWidth = video_mode.resolutionX - frame->width;

  if (mirroring)
  {
    target += frame->width;

    for (int y = 0; y < frame->height; y++)
    {
      for (int x = 0; x < frame->width; x++)
      {
        *target-- = *source++;
      }

      source += skipWidth;
      target += 2 * frame->width;
    }
  }
  else
  {
    for (int y = 0; y < frame->height; y++)
    {
      for (int x = 0; x < frame->width; x++)
      {
        *target++ = *source++;
      }

      source += skipWidth;
    }
  }

  /*
  uint16_t* data_ptr = static_cast<uint16_t*>(data);
  uint16_t* frame_data = static_cast<uint16_t*>(frame->data);
  if (mirroring)
  {
    for (unsigned int i = 0; i < frame->dataSize / 2; i++)
    {
      // find corresponding mirrored pixel
      unsigned int row = i / video_mode.resolutionX;
      unsigned int col = video_mode.resolutionX - (i % video_mode.resolutionX);
      unsigned int target = (row * video_mode.resolutionX) + col;
      // copy it to this pixel
      frame_data[i] = data_ptr[target];
    }
  }
  else
    std::copy(data_ptr, data_ptr+frame->dataSize / 2, frame_data);
  */
}


/* depth video modes reference

FREENECT_DEPTH_11BIT        = 0, //< 11 bit depth information in one uint16_t/pixel
FREENECT_DEPTH_10BIT        = 1, //< 10 bit depth information in one uint16_t/pixel
FREENECT_DEPTH_11BIT_PACKED = 2, //< 11 bit packed depth information
FREENECT_DEPTH_10BIT_PACKED = 3, //< 10 bit packed depth information
FREENECT_DEPTH_REGISTERED   = 4, //< processed depth data in mm, aligned to 640x480 RGB
FREENECT_DEPTH_MM           = 5, //< depth to each pixel in mm, but left unaligned to RGB image
FREENECT_DEPTH_DUMMY        = 2147483647, //< Dummy value to force enum to be 32 bits wide

ONI_PIXEL_FORMAT_DEPTH_1_MM = 100,
ONI_PIXEL_FORMAT_DEPTH_100_UM = 101,
ONI_PIXEL_FORMAT_SHIFT_9_2 = 102,
ONI_PIXEL_FORMAT_SHIFT_9_3 = 103,
*/
