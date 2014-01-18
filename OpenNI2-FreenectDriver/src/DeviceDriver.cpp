/**
*  FreenectDriver
*  Copyright 2013 Benn Snyder <benn.snyder@gmail.com>
*
*  Licensed under the Apache License, Version 2.0 (the "License");
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
*  Unless required by applicable law or agreed to in writing, software
*  distributed under the License is distributed on an "AS IS" BASIS,
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*  limitations under the License.
*/
#include <map>
#include <sstream>
#include "Driver/OniDriverAPI.h"
#include "libfreenect.hpp"
#include "DepthStream.hpp"
#include "ColorStream.hpp"


static bool operator<(const OniDeviceInfo& left, const OniDeviceInfo& right) { return (strcmp(left.uri, right.uri) < 0); } // for std::map

namespace FreenectDriver {
  class Device : public oni::driver::DeviceBase, public Freenect::FreenectDevice {
  private:
    ColorStream* color;
    DepthStream* depth;

    // for Freenect::FreenectDevice
    void DepthCallback(void* data, uint32_t timestamp) {
      depth->buildFrame(data, timestamp);
    }
    void VideoCallback(void* data, uint32_t timestamp) {
      color->buildFrame(data, timestamp);
    }

  public:
    Device(freenect_context* fn_ctx, int index) : Freenect::FreenectDevice(fn_ctx, index),
      color(NULL),
      depth(NULL) { }
    ~Device() {
      destroyStream(color);
      destroyStream(depth);
    }

    // for DeviceBase

    OniBool isImageRegistrationModeSupported(OniImageRegistrationMode mode) { return depth->isImageRegistrationModeSupported(mode); }

    OniStatus getSensorInfoList(OniSensorInfo** pSensors, int* numSensors) {
      *numSensors = 2;
      OniSensorInfo * sensors = new OniSensorInfo[*numSensors];
      sensors[0] = depth->getSensorInfo();
      sensors[1] = color->getSensorInfo();
      *pSensors = sensors;
      return ONI_STATUS_OK;
    }

    oni::driver::StreamBase* createStream(OniSensorType sensorType) {
      switch (sensorType) {
        default:
          //m_driverServices.errorLoggerAppend("FreenectDeviceNI: Can't create a stream of type %d", sensorType);
          return NULL;
        case ONI_SENSOR_COLOR:
          Freenect::FreenectDevice::startVideo();
          if (! color)
            color = new ColorStream(this);
          return color;
        case ONI_SENSOR_DEPTH:
          Freenect::FreenectDevice::startDepth();
          if (! depth)
            depth = new DepthStream(this);
          return depth;
        // todo: IR
      }
    }

    void destroyStream(oni::driver::StreamBase* pStream) {
      if (! pStream)
        return;

      if (pStream == color) {
        Freenect::FreenectDevice::stopVideo();
        delete color;
        color = NULL;
      }
      if (pStream == depth) {
        Freenect::FreenectDevice::stopDepth();
        delete depth;
        depth = NULL;
      }
    }

    // todo: fill out properties
    OniBool isPropertySupported(int propertyId)
    {
      if(propertyId == ONI_DEVICE_PROPERTY_IMAGE_REGISTRATION)
        return true;
      return false;
    }

    OniStatus getProperty(int propertyId, void* data, int* pDataSize)
    {
      switch (propertyId) {
        default:
        case ONI_DEVICE_PROPERTY_FIRMWARE_VERSION:        // string
        case ONI_DEVICE_PROPERTY_DRIVER_VERSION:          // OniVersion
        case ONI_DEVICE_PROPERTY_HARDWARE_VERSION:        // int
        case ONI_DEVICE_PROPERTY_SERIAL_NUMBER:           // string
        case ONI_DEVICE_PROPERTY_ERROR_STATE:             // ?
        // files
        case ONI_DEVICE_PROPERTY_PLAYBACK_SPEED:          // float
        case ONI_DEVICE_PROPERTY_PLAYBACK_REPEAT_ENABLED: // OniBool
        // xn
        case XN_MODULE_PROPERTY_USB_INTERFACE:            // XnSensorUsbInterface
        case XN_MODULE_PROPERTY_MIRROR:                   // bool
        case XN_MODULE_PROPERTY_RESET_SENSOR_ON_STARTUP:  // unsigned long long
        case XN_MODULE_PROPERTY_LEAN_INIT:                // unsigned long long
        case XN_MODULE_PROPERTY_SERIAL_NUMBER:            // unsigned long long
        case XN_MODULE_PROPERTY_VERSION:                  // XnVersions
          return ONI_STATUS_NOT_SUPPORTED;

        case ONI_DEVICE_PROPERTY_IMAGE_REGISTRATION:      // OniImageRegistrationMode
          if (*pDataSize != sizeof(OniImageRegistrationMode)) {
            printf("Unexpected size: %d != %lu\n", *pDataSize, sizeof(OniImageRegistrationMode));
            return ONI_STATUS_ERROR;
          }
          *(static_cast<OniImageRegistrationMode*>(data)) = depth->getImageRegistrationMode();
          return ONI_STATUS_OK;
      }
    }
    OniStatus setProperty(int propertyId, const void* data, int dataSize)
    {
      switch (propertyId) {
        default:
        case ONI_DEVICE_PROPERTY_FIRMWARE_VERSION:        // By implementation
        case ONI_DEVICE_PROPERTY_DRIVER_VERSION:          // OniVersion
        case ONI_DEVICE_PROPERTY_HARDWARE_VERSION:        // int
        case ONI_DEVICE_PROPERTY_SERIAL_NUMBER:           // string
        case ONI_DEVICE_PROPERTY_ERROR_STATE:             // ?
        // files
        case ONI_DEVICE_PROPERTY_PLAYBACK_SPEED:          // float
        case ONI_DEVICE_PROPERTY_PLAYBACK_REPEAT_ENABLED: // OniBool
        // xn
        case XN_MODULE_PROPERTY_USB_INTERFACE:            // XnSensorUsbInterface
        case XN_MODULE_PROPERTY_MIRROR:                   // bool
        case XN_MODULE_PROPERTY_RESET_SENSOR_ON_STARTUP:  // unsigned long long
        case XN_MODULE_PROPERTY_LEAN_INIT:                // unsigned long long
        case XN_MODULE_PROPERTY_SERIAL_NUMBER:            // unsigned long long
        case XN_MODULE_PROPERTY_VERSION:                  // XnVersions
        // xn commands
        case XN_MODULE_PROPERTY_FIRMWARE_PARAM:           // XnInnerParam
        case XN_MODULE_PROPERTY_RESET:                    // unsigned long long
        case XN_MODULE_PROPERTY_IMAGE_CONTROL:            // XnControlProcessingData
        case XN_MODULE_PROPERTY_DEPTH_CONTROL:            // XnControlProcessingData
        case XN_MODULE_PROPERTY_AHB:                      // XnAHBData
        case XN_MODULE_PROPERTY_LED_STATE:                // XnLedState
          return ONI_STATUS_NOT_SUPPORTED;

        case ONI_DEVICE_PROPERTY_IMAGE_REGISTRATION:      // OniImageRegistrationMode
          if (dataSize != sizeof(OniImageRegistrationMode)) {
            printf("Unexpected size: %d != %lu\n", dataSize, sizeof(OniImageRegistrationMode));
            return ONI_STATUS_ERROR;
          }
          return depth->setImageRegistrationMode(*(static_cast<const OniImageRegistrationMode*>(data)));
      }
    }

    OniBool isCommandSupported(int commandId) //{ return (invoke(propertyId, NULL, sizeof(NULL)) != ONI_STATUS_NOT_SUPPORTED); }
    {
      switch (commandId)
      {
        default:
        case ONI_DEVICE_COMMAND_SEEK:
          return false;
      }
    }
    
    OniStatus invoke(int commandId, void* data, int dataSize)
    {
      switch (commandId)
      {
        default:
        case ONI_DEVICE_COMMAND_SEEK: // OniSeek
          return ONI_STATUS_NOT_SUPPORTED;
      }
    }


    /* todo: for DeviceBase
    virtual OniStatus tryManualTrigger() {return ONI_STATUS_OK;}
    */
  };


  class Driver : public oni::driver::DriverBase, private Freenect::Freenect {
  private:
    std::map<OniDeviceInfo, oni::driver::DeviceBase*> devices;

  public:
    Driver(OniDriverServices* pDriverServices) : DriverBase(pDriverServices) {
      //freenect_set_log_level(m_ctx, FREENECT_LOG_NOTICE);
      freenect_select_subdevices(m_ctx, FREENECT_DEVICE_CAMERA); // OpenNI2 doesn't use MOTOR
    }
    ~Driver() { shutdown(); }

    // for DriverBase

    OniStatus initialize(oni::driver::DeviceConnectedCallback connectedCallback, oni::driver::DeviceDisconnectedCallback disconnectedCallback, oni::driver::DeviceStateChangedCallback deviceStateChangedCallback, void* pCookie) {
      DriverBase::initialize(connectedCallback, disconnectedCallback, deviceStateChangedCallback, pCookie);
      for (int i = 0; i < Freenect::deviceCount(); ++i) {
        std::ostringstream uri;
        uri << "freenect://" << i;
        OniDeviceInfo info;
        strncpy(info.uri, uri.str().c_str(), ONI_MAX_STR);
        strncpy(info.vendor, "Microsoft", ONI_MAX_STR);
        strncpy(info.name, "Kinect", ONI_MAX_STR);
        devices[info] = NULL;
        deviceConnected(&info);
        deviceStateChanged(&info, 0);
      }
      return ONI_STATUS_OK;
    }

    oni::driver::DeviceBase* deviceOpen(const char* uri, const char* mode = NULL) {
      for (std::map<OniDeviceInfo, oni::driver::DeviceBase*>::iterator iter = devices.begin(); iter != devices.end(); ++iter) {
        if (strcmp(iter->first.uri, uri) == 0) { // found
          if (iter->second) // already open
            return iter->second;
          else {
            unsigned int id;
            std::istringstream is(iter->first.uri);
            is.seekg(strlen("freenect://"));
            is >> id;
            Device* device = &createDevice<Device>(id);
            iter->second = device;
            return device;
          }
        }
      }

      getServices().errorLoggerAppend("Could not find device '%s'", uri);
      return NULL;
    }

    void deviceClose(oni::driver::DeviceBase* pDevice) {
      for (std::map<OniDeviceInfo, oni::driver::DeviceBase*>::iterator iter = devices.begin(); iter != devices.end(); ++iter) {
        if (iter->second == pDevice) {
          iter->second = NULL;
          unsigned int id;
          std::istringstream is(iter->first.uri);
          is.seekg(strlen("freenect://"));
          is >> id;
          deleteDevice(id);
          return;
        }
      }

      getServices().errorLoggerAppend("Could not close unrecognized device");
    }

    OniStatus tryDevice(const char* uri) {
      oni::driver::DeviceBase* device = deviceOpen(uri);
      if (! device)
        return ONI_STATUS_ERROR;
      deviceClose(device);
      return ONI_STATUS_OK;
    }

    void shutdown() {
      for (std::map<OniDeviceInfo, oni::driver::DeviceBase*>::iterator iter = devices.begin(); iter != devices.end(); ++iter)
        deviceClose(iter->second);
    }


    /* todo: for DriverBase
    virtual void* enableFrameSync(oni::driver::StreamBase** pStreams, int streamCount);
    virtual void disableFrameSync(void* frameSyncGroup);
    */
  };
}


// macros defined in XnLib (not included) - workaround
#define XN_NEW(type, arg...) new type(arg)
#define XN_DELETE(p) delete(p)
ONI_EXPORT_DRIVER(FreenectDriver::Driver);
#undef XN_NEW
#undef XN_DELETE
