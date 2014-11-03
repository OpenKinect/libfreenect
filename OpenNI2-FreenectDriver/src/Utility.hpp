// This file contains symbols that may be used by any class or don't really go anywhere else.
#pragma once

#include <iostream>
#include "Driver/OniDriverAPI.h"


// Oni helpers

static OniVideoMode makeOniVideoMode(OniPixelFormat pixel_format, int resolution_x, int resolution_y, int frames_per_second)
{
  OniVideoMode mode;
  mode.pixelFormat = pixel_format;
  mode.resolutionX = resolution_x;
  mode.resolutionY = resolution_y;
  mode.fps = frames_per_second;
  return mode;
}
static bool operator==(const OniVideoMode& left, const OniVideoMode& right)
{
  return (left.pixelFormat == right.pixelFormat && left.resolutionX == right.resolutionX
          && left.resolutionY == right.resolutionY && left.fps == right.fps);
}
static bool operator<(const OniVideoMode& left, const OniVideoMode& right)
{
  return (left.resolutionX * left.resolutionY < right.resolutionX * right.resolutionY);
}

static bool operator<(const OniDeviceInfo& left, const OniDeviceInfo& right)
{
  return (strcmp(left.uri, right.uri) < 0);
}


/// Extracts `first` from `pair`, for transforming a map into its keys.
struct ExtractKey
{
  template <typename T> typename T::first_type
  operator()(T pair) const
  {
    return pair.first;
  }
};


// holding out on C++11
template <typename T>
static std::string to_string(const T& n)
{
  std::ostringstream oss;
  oss << n;
  return oss.str();
}


// global logging
namespace FreenectDriver
{
  static void WriteMessage(std::string info)
  {
    std::cout << "OpenNI2-FreenectDriver: " << info << std::endl;
  }
  
  // DriverServices is set in DeviceDriver.cpp so all files can call errorLoggerAppend()
  static oni::driver::DriverServices* DriverServices;
  static void LogError(std::string error)
  {
    // errorLoggerAppend() doesn't seem to go anywhere, so WriteMessage also
    WriteMessage("(ERROR) " + error);
    
    if (DriverServices != NULL)
      DriverServices->errorLoggerAppend(std::string("OpenNI2-FreenectDriver: " + error).c_str());
  }
}
