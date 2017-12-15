#pragma once

#include "libfreenect.hpp"
#include "PS1080.h"
#include "Utility.hpp"


namespace FreenectDriver
{
  class VideoStream : public oni::driver::StreamBase
  {
  private:
    unsigned int frame_id; // number each frame
    uint64_t prev_timestamp; // timestamp of the last processed frame

    virtual OniStatus setVideoMode(OniVideoMode requested_mode) = 0;
    virtual void populateFrame(void* data, OniFrame* frame) const = 0;

  protected:
    Freenect::FreenectDevice* device;
    bool running; // buildFrame() does something iff true
    OniVideoMode video_mode;
    OniCropping cropping;
    bool mirroring;

  public:
    VideoStream(Freenect::FreenectDevice* device) :
      frame_id(1),
      prev_timestamp(0),
      device(device),
      running(false),
      mirroring(false)
      {
        // joy of structs
        memset(&cropping, 0, sizeof(cropping));
        memset(&video_mode, 0, sizeof(video_mode));
      }
    //~VideoStream() { stop();  }

    void buildFrame(void* data, uint32_t timestamp)
    {
      if (!running)
        return;

      OniFrame* frame = getServices().acquireFrame();
      frame->frameIndex = frame_id++;
      frame->videoMode = video_mode;
      frame->width = video_mode.resolutionX;
      frame->height = video_mode.resolutionY;

      // Handle overflow, input timestamp comes from a 60MHz clock and overflows
      // in ~70s
      if (timestamp < prev_timestamp)
      {
        uint32_t prev_int = static_cast<uint32_t>(prev_timestamp);
        uint64_t temp_delta = timestamp - prev_int;
        prev_timestamp += temp_delta;
      } else {
        prev_timestamp = timestamp;
      }

      // OpenNI wants the value in microseconds
      frame->timestamp = prev_timestamp / 60;

      populateFrame(data, frame);
      raiseNewFrame(frame);
      getServices().releaseFrame(frame);
    }

    // from StreamBase

    OniStatus start()
    {
      running = true;
      return ONI_STATUS_OK;
    }
    void stop() { running = false; }

    // only add to property handlers if the property is generic to all children
    // otherwise, implement in child and call these in default case
    OniBool isPropertySupported(int propertyId)
    {
      switch(propertyId)
      {
        case ONI_STREAM_PROPERTY_VIDEO_MODE:
        case ONI_STREAM_PROPERTY_CROPPING:
        case ONI_STREAM_PROPERTY_MIRRORING:
          return true;
        default:
          return false;
      }
    }

    virtual OniStatus getProperty(int propertyId, void* data, int* pDataSize)
    {
      switch (propertyId)
      {
        default:
        case ONI_STREAM_PROPERTY_HORIZONTAL_FOV:      // float: radians
        case ONI_STREAM_PROPERTY_VERTICAL_FOV:        // float: radians
        case ONI_STREAM_PROPERTY_MAX_VALUE:           // int
        case ONI_STREAM_PROPERTY_MIN_VALUE:           // int
        case ONI_STREAM_PROPERTY_STRIDE:              // int
        case ONI_STREAM_PROPERTY_NUMBER_OF_FRAMES:    // int
        // camera
        case ONI_STREAM_PROPERTY_AUTO_WHITE_BALANCE:  // OniBool
        case ONI_STREAM_PROPERTY_AUTO_EXPOSURE:       // OniBool
        // xn
        case XN_STREAM_PROPERTY_INPUT_FORMAT:         // unsigned long long
        case XN_STREAM_PROPERTY_CROPPING_MODE:        // XnCroppingMode
          return ONI_STATUS_NOT_SUPPORTED;

        case ONI_STREAM_PROPERTY_VIDEO_MODE:          // OniVideoMode*
          if (*pDataSize != sizeof(OniVideoMode))
          {
            LogError("Unexpected size for ONI_STREAM_PROPERTY_VIDEO_MODE");
            return ONI_STATUS_ERROR;
          }
          *(static_cast<OniVideoMode*>(data)) = video_mode;
          return ONI_STATUS_OK;

        case ONI_STREAM_PROPERTY_CROPPING:            // OniCropping*
          if (*pDataSize != sizeof(OniCropping))
          {
            LogError("Unexpected size for ONI_STREAM_PROPERTY_CROPPING");
            return ONI_STATUS_ERROR;
          }
          *(static_cast<OniCropping*>(data)) = cropping;
          return ONI_STATUS_OK;

        case ONI_STREAM_PROPERTY_MIRRORING:           // OniBool
          if (*pDataSize != sizeof(OniBool))
          {
            LogError("Unexpected size for ONI_STREAM_PROPERTY_MIRRORING");
            return ONI_STATUS_ERROR;
          }
          *(static_cast<OniBool*>(data)) = mirroring;
          return ONI_STATUS_OK;
      }
    }
    virtual OniStatus setProperty(int propertyId, const void* data, int dataSize)
    {
      switch (propertyId)
      {
        default:
        case ONI_STREAM_PROPERTY_HORIZONTAL_FOV:      // float: radians
        case ONI_STREAM_PROPERTY_VERTICAL_FOV:        // float: radians
        case ONI_STREAM_PROPERTY_MAX_VALUE:           // int
        case ONI_STREAM_PROPERTY_MIN_VALUE:           // int
        case ONI_STREAM_PROPERTY_STRIDE:              // int
        case ONI_STREAM_PROPERTY_NUMBER_OF_FRAMES:    // int
        // camera
        case ONI_STREAM_PROPERTY_AUTO_WHITE_BALANCE:  // OniBool
        case ONI_STREAM_PROPERTY_AUTO_EXPOSURE:       // OniBool
        // xn
        case XN_STREAM_PROPERTY_INPUT_FORMAT:         // unsigned long long
        case XN_STREAM_PROPERTY_CROPPING_MODE:        // XnCroppingMode
          return ONI_STATUS_NOT_SUPPORTED;

        case ONI_STREAM_PROPERTY_VIDEO_MODE:          // OniVideoMode*
          if (dataSize != sizeof(OniVideoMode))
          {
            LogError("Unexpected size for ONI_STREAM_PROPERTY_VIDEO_MODE");
            return ONI_STATUS_ERROR;
          }
          if (ONI_STATUS_OK != setVideoMode(*(static_cast<const OniVideoMode*>(data))))
            return ONI_STATUS_NOT_SUPPORTED;
          raisePropertyChanged(propertyId, data, dataSize);
          return ONI_STATUS_OK;

        case ONI_STREAM_PROPERTY_CROPPING:            // OniCropping*
          if (dataSize != sizeof(OniCropping))
          {
            LogError("Unexpected size for ONI_STREAM_PROPERTY_CROPPING");
            return ONI_STATUS_ERROR;
          }
          cropping = *(static_cast<const OniCropping*>(data));
          raisePropertyChanged(propertyId, data, dataSize);
          return ONI_STATUS_OK;

        case ONI_STREAM_PROPERTY_MIRRORING:           // OniBool
          if (dataSize != sizeof(OniBool))
          {
            LogError("Unexpected size for ONI_STREAM_PROPERTY_MIRRORING");
            return ONI_STATUS_ERROR;
          }
          mirroring = *(static_cast<const OniBool*>(data));
          raisePropertyChanged(propertyId, data, dataSize);
          return ONI_STATUS_OK;
      }
    }


    /* todo : from StreamBase
    virtual OniStatus convertDepthToColorCoordinates(StreamBase* colorStream, int depthX, int depthY, OniDepthPixel depthZ, int* pColorX, int* pColorY) { return ONI_STATUS_NOT_SUPPORTED; }
    */
  };
}


/* image video modes reference

FREENECT_VIDEO_RGB             = 0, //< Decompressed RGB mode (demosaicing done by libfreenect)
FREENECT_VIDEO_BAYER           = 1, //< Bayer compressed mode (raw information from camera)
FREENECT_VIDEO_IR_8BIT         = 2, //< 8-bit IR mode
FREENECT_VIDEO_IR_10BIT        = 3, //< 10-bit IR mode
FREENECT_VIDEO_IR_10BIT_PACKED = 4, //< 10-bit packed IR mode
FREENECT_VIDEO_YUV_RGB         = 5, //< YUV RGB mode
FREENECT_VIDEO_YUV_RAW         = 6, //< YUV Raw mode
FREENECT_VIDEO_DUMMY           = 2147483647, //< Dummy value to force enum to be 32 bits wide

ONI_PIXEL_FORMAT_RGB888 = 200,
ONI_PIXEL_FORMAT_YUV422 = 201,
ONI_PIXEL_FORMAT_GRAY8 = 202,
ONI_PIXEL_FORMAT_GRAY16 = 203,
ONI_PIXEL_FORMAT_JPEG = 204,
*/
