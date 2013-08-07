/*
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2013 individual OpenKinect contributors. See the CONTRIB file
 * for details.
 *
 * This code is licensed to you under the terms of the Apache License, version
 * 2.0, or, at your option, the terms of the GNU General Public License,
 * version 2.0. See the APACHE20 and GPL2 files for the text of the licenses,
 * or the following URLs:
 * http://www.apache.org/licenses/LICENSE-2.0
 * http://www.gnu.org/licenses/gpl-2.0.txt
 *
 * If you redistribute this file in source form, modified or unmodified, you
 * may:
 *   1) Leave this header intact and distribute it under the same terms,
 *      accompanying it with the APACHE20 and GPL20 files, or
 *   2) Delete the Apache 2.0 clause and accompany it with the GPL2 file, or
 *   3) Delete the GPL v2 clause and accompany it with the APACHE20 file
 * In all cases you must keep the copyright notice intact and include a copy
 * of the CONTRIB file.
 *
 * Binary distributions must follow the binary distribution requirements of
 * either License.
 */
#pragma once

#include <csignal>
#include <thread>
#include <boost/signals2.hpp>
#include <libfreenect.h>


/// This file is a structured, object-oriented wrapper around libfreenect.
/// It reduces boilerplate code and attemps to handle hardware access gracefully - you do not need to:
///   * initialize a context
///   * manage devices
///   * start and stop streams
/// A freenect_exception is thrown whenever a libfreenect function returns an error.
///
/// Start by calling devices() to obtain a vector of shared pointers to Device instances.
/// Each entry in the vector corresponds to a connected libfreenect device.
/// Each Device has a VideoStream `video` and a DepthStream `depth`.
/// Some `device`s might not have motor support (e.g. K4W); in that case `device->motor == nullptr`
///
/// VideoStream and DepthStream do not use callbacks; they relay each VideoStream::Frame or DepthStream::Frame to your application using signals and slots.
/// Use VideoStream::connect(VideoStream::FrameSignal) or DepthStream::connect(DepthStream::FrameSignal) to accept frames from a stream.
/// A VideoStream::FrameSignal is a boost::signals2::signal with the signature `void (VideoStream::Frame)`.
/// cpptest.cpp shows how to use the signal/slot pattern. See also the documentation for VideoStream::connect().

// If you are reading for comprehension, start in the Device class and work your way up.
// If you are feeling adventurous, tackle the runloop logic in the anonymous namespace at the end.

/// The Freenect namespace provides freenect_exception and defines four basic concepts: Device, Motor, DepthStream, and VideoStream.
namespace Freenect {
  static freenect_context* fn_context = nullptr;

  /// A freenect_exception may be thrown whenever a libfreenect function returns an error (negative value).
  class freenect_exception : public std::exception {
  public:
    const int error;
    const std::string function;

    /// @param error libfreenect error code
    /// @param function Function that returned error (optional)
    freenect_exception(int error, std::string function = "") throw() :
    error(error),
    function(function) { }
    ~freenect_exception() throw() { }

    virtual const char* what() const throw() {
      std::string message = "A call to ";
      message += function.empty() ? "a freenect function" : function;
      message += " returned " + error;
      return message.c_str();
    }
  };
  
  /// Applications interested in receiving depth data should connect() a slot where frames will be sent.
  class DepthStream {
  public:
    /// A DepthStream::Frame is a single frame of depth values in millimeters, aligned to its corresponding VideoStream::Frame.
    class Frame {
    public:
      const freenect_frame_mode mode;
      const uint32_t timestamp;
      std::vector<uint16_t> data;
    
      Frame(freenect_frame_mode mode, uint32_t timestamp, uint16_t* data_ptr) :
        mode(mode),
        timestamp(timestamp),
        data(data_ptr, data_ptr + (mode.bytes/2)) { }
      //~Frame();
      
      /// Each pixel is the same grayscale value three times.
      /// @return RGB-encoded reprensentation of this frame's buffer
      std::vector<unsigned char> rgb() const {
        std::vector<unsigned char> rgb_pixels(3 * data.size());
        for (unsigned int i = 0; i < data.size(); ++i)
          rgb_pixels[3*i] = rgb_pixels[3*i+1] = rgb_pixels[3*i+2] = 256 * (FREENECT_DEPTH_MM_MAX_VALUE - data.at(i) / static_cast<float>(FREENECT_DEPTH_MM_MAX_VALUE));
        return rgb_pixels;
      }
    };

    /// A FrameSignal will be triggered from another thread whenever there is a new Frame.
    typedef boost::signals2::signal<void (Frame)> FrameSignal;
  
    DepthStream(freenect_device* fn_device) :
      fn_device(fn_device),
      running(false) {
      int ret = freenect_set_depth_mode(fn_device, freenect_find_depth_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_REGISTERED));
      if (ret < 0)
        throw freenect_exception(ret, "freenect_set_depth_mode");
        
      //setMirroring(false);
      freenect_set_depth_callback(fn_device, &freenectCallback);
    }
    ~DepthStream() { stop(); }

    freenect_depth_format format() const { return freenect_get_current_depth_mode(fn_device).depth_format; }
    unsigned int framerate() const { return freenect_get_current_depth_mode(fn_device).framerate; }
    freenect_resolution resolution() const { return freenect_get_current_video_mode(fn_device).resolution; }
    
    /// @param mirror true for on; false for off
    //void setMirroring(bool mirror) { }//freenect_set_depth_mirroring(fn_device, static_cast<freenect_mirroring_flag>(mirror)); }

    /// @param res Desired resolution
    /// @throw freenect_exception A call to a freenect function returned an error
    void setResolution(freenect_resolution res) {
      bool resume = running;
      stop();

      int ret = freenect_set_depth_mode(fn_device, freenect_find_depth_mode(res, freenect_get_current_depth_mode(fn_device).depth_format));
      if (ret < 0)
        throw freenect_exception(ret, "freenect_set_depth_mode");
      
      if (resume)
        start();
    }
    
    /// Connect a slot to a FrameSignal that is triggered for each frame.
    /// The caller is responsible for disconnecting when done, by doing one of the following:
    /// 1) Call the boost::signals2::connection::disconnect() method on the returned connection
    /// 2) Store the returned connection in a boost:signals2::scoped_connection
    /// 3) Use boost::signals2::slot::track() to disconnect when your object goes out of scope
    /// See http://www.boost.org/doc/libs/1_53_0/doc/html/signals2/tutorial.html#idp161508624
    /// @param slot Slot to connect
    /// @return Connection 
    /// @throw freenect_exception A call to a freenect function returned an error
    boost::signals2::connection connect(const FrameSignal::slot_type slot) {
      boost::signals2::connection connection = publish.connect(slot);
      start();
      return connection;
    }
    
    /// Stop the stream iff there are no slots (no one's listening for frames).
    /// @throw freenect_exception A call to a freenect function returned an error
    void verifySlots() {
      if (publish.empty())
        stop();
    }

  private:
    freenect_device* fn_device;
    FrameSignal publish;
    bool running;

    static void freenectCallback(freenect_device* fn_device, void* depth, uint32_t timestamp);
    
    /// @throw freenect_exception A call to a freenect function returned an error
    void start() {
      if (running)
        return;
        
      int ret = freenect_start_depth(fn_device);
      if (ret < 0)
        throw freenect_exception(ret, "freenect_start_depth");
        
      running = true;
    }
    /// @throw freenect_exception A call to a freenect function returned an error
    void stop() {
      if (! running)
        return;
      
      int ret = freenect_stop_depth(fn_device);
      if (ret < 0)
        throw freenect_exception(ret, "freenect_stop_depth");
      
      running = false;
    }
  };

  /// Applications interested in receiving video data should connect() a slot where frames will be sent.
  class VideoStream {
  public:
    /// A VideoStream::Frame is a single frame of Bayer-encoded color values.
    class Frame {
    public:
      const freenect_frame_mode mode;
      const uint32_t timestamp;
      std::vector<uint8_t> data;
    
      Frame(freenect_frame_mode mode, uint32_t timestamp, uint8_t* data_ptr) :
        mode(mode),
        timestamp(timestamp),
        data(data_ptr, data_ptr + mode.bytes) { }
      //~Frame();
    };
    
    /// A FrameSignal will be triggered from another thread whenever there is a new Frame.
    typedef boost::signals2::signal<void (Frame)> FrameSignal;
    
    /// @throw freenect_exception A call to a freenect function returned an error
    VideoStream(freenect_device* fn_device) :
      fn_device(fn_device),
      running(false) {
      int ret = freenect_set_video_mode(fn_device, freenect_find_video_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_BAYER));
      if (ret < 0)
        throw freenect_exception(ret, "freenect_set_video_mode");

      //setMirroring(false);
      freenect_set_video_callback(fn_device, &freenectCallback);
    }
    ~VideoStream() { stop(); }
    
    unsigned int framerate() const { return freenect_get_current_video_mode(fn_device).framerate; }
    
    /// @param mirror true for on; false for off
    //void setMirroring(bool mirror) { }// freenect_set_video_mirroring(fn_device, static_cast<freenect_mirroring_flag>(mirror)); }

    /// @param res Desired resolution
    /// @throw freenect_exception A call to a freenect function returned an error
    void setResolution(freenect_resolution res) {
      bool resume = running;
      stop();

      int ret = freenect_set_video_mode(fn_device, freenect_find_video_mode(res, freenect_get_current_video_mode(fn_device).video_format));
      if (ret < 0)
        throw freenect_exception(ret, "freenect_set_video_mode");
      
      if (resume)
        start();
    }
    
    /// Connect a slot to a FrameSignal that is triggered for each frame.
    /// The caller is responsible for disconnecting when done, by doing one of the following:
    /// 1) Call the boost::signals2::connection::disconnect() method on the returned connection
    /// 2) Store the returned connection in a boost:signals2::scoped_connection
    /// 3) Use boost::signals2::slot::track() to disconnect when your object goes out of scope
    /// See http://www.boost.org/doc/libs/1_53_0/doc/html/signals2/tutorial.html#idp161508624
    /// @param slot Slot to connect
    /// @return Connection 
    /// @throw freenect_exception A call to a freenect function returned an error
    boost::signals2::connection connect(const FrameSignal::slot_type slot) {
      boost::signals2::connection connection = publish.connect(slot);
      start();
      return connection;
    }
    
    /// Stop the stream iff there are no slots (no one's listening for frames).
    /// @throw freenect_exception A call to a freenect function returned an error
    void verifySlots() {
      if (publish.empty())
        stop();
    }
    
  private:
    freenect_device* fn_device;
    FrameSignal publish;
    bool running;
    
    static void freenectCallback(freenect_device* fn_device, void* color, uint32_t timestamp);

    /// @throw freenect_exception A call to a freenect function returned an error
    void start() {
      if (running)
        return;
        
      int ret = freenect_start_video(fn_device);
      if (ret < 0)
        throw freenect_exception(ret, "freenect_start_video");
        
      running = true;
    }
    /// @throw freenect_exception A call to a freenect function returned an error
    void stop() {
      if (! running)
        return;
      
      int ret = freenect_stop_video(fn_device);
      if (ret < 0)
        throw freenect_exception(ret, "freenect_stop_video");
      
      running = false;
    }
  };

  /// A Motor encapsulates a Device's tilt motor and LED.
  class Motor {
  public:
    static const int MAX_ANGLE = 31;
    static const int MIN_ANGLE = -31;

    Motor(freenect_device* fn_device):
      fn_device(fn_device) { }
    ~Motor() {
      setAngle(0);
      setLED(LED_BLINK_GREEN);
    }
    
    /// @return Current tilt motor strain - 0 is no strain
    //virtual int strain() const = 0;

    /// @return Current accelerometer readings
    //virtual std::tuple<double, double, double> accelerometers() const = 0;

    /// @return true iff motor is moving
    /// @throw freenect_exception A call to a freenect function returned an error
    bool moving() const {
      int ret = freenect_update_tilt_state(fn_device);
      if (ret < 0)
        throw freenect_exception(ret, "freenect_update_tilt_state");
      
      return (freenect_get_tilt_status(freenect_get_tilt_state(fn_device)) != TILT_STATUS_STOPPED);
    }

    /// @return Current tilt angle in degrees
    /// @throw freenect_exception A call to a freenect function returned an error
    double angle() const {
      int ret = freenect_update_tilt_state(fn_device);
      if (ret < 0)
        throw freenect_exception(ret, "freenect_update_tilt_state");
        
      return freenect_get_tilt_degs(freenect_get_tilt_state(fn_device));
    }

    /// Limited between MIN_ANGLE and MAX_ANGLE
    /// @param angle Requested tilt angle in degrees
    /// @throw freenect_exception A call to a freenect function returned an error
    void setAngle(double angle) {
      int ret = freenect_set_tilt_degs(fn_device, angle);
      if (ret < 0)
        throw freenect_exception(ret, "freenect_set_tilt_degs");
    }

    /// @param status Requested LED status
    /// @throw freenect_exception A call to a freenect function returned an error
    void setLED(freenect_led_options status) {
      int ret = freenect_set_led(fn_device, status);
      if (ret < 0)
        throw freenect_exception(ret, "freenect_set_led");
    }

  private:
    freenect_device* fn_device;
  }; const int Motor::MIN_ANGLE; const int Motor::MAX_ANGLE;
  
  /// A Device encapsulates the capabilities of a Freenect sensor.
  /// Applications should not depend the existence of motor; it will be nullptr for a K4W model.
  class Device {
  public:
    std::unique_ptr<DepthStream> depth;
    std::unique_ptr<VideoStream> video;
    std::unique_ptr<Motor> motor;
    const unsigned int index;
    const std::string serial;

    /// @throw freenect_exception A call to a freenect function returned an error
    Device(unsigned int index, std::string serial) :
      index(index),
      serial(serial),
      fn_device(nullptr) { }
      
    void load() {
      if (fn_device) return;

      freenect_select_subdevices(fn_context, SUBDEVICES);
      
      int ret = freenect_open_device(fn_context, &fn_device, index);
      if (ret < 0) {
        printf("Couldn't open device with all supported subdevices, dropping FREENECT_DEVICE_MOTOR and retrying\n");
        freenect_select_subdevices(fn_context, static_cast<freenect_device_flags>(SUBDEVICES & ~FREENECT_DEVICE_MOTOR));
        ret = freenect_open_device(fn_context, &fn_device, index);
        if (ret < 0)
          throw freenect_exception(ret, "freenect_open_device");
        motor.reset(nullptr);
      } else {
        motor.reset(new Motor(fn_device));
      }
      video.reset(new VideoStream(fn_device));
      depth.reset(new DepthStream(fn_device));
      
      freenect_set_user(fn_device, this);
    }
    /// @throw freenect_exception A call to a freenect function returned an error
    ~Device() {
      // destroy subdevices before closing device
      delete depth.release();
      delete video.release();
      delete motor.release();
      
      if (fn_device) {
        int ret = freenect_close_device(fn_device);
        if (ret < 0)
          throw freenect_exception(ret, "freenect_close_device");
      }
    }
    
  private:
    bool loaded;
    freenect_device* fn_device;
    static const freenect_device_flags SUBDEVICES = static_cast<freenect_device_flags>(FREENECT_DEVICE_CAMERA | FREENECT_DEVICE_MOTOR);
  };
  
  class device_ptr : public std::shared_ptr<Device> {
  public:
    device_ptr(unsigned int index, std::string serial) : std::shared_ptr<Device>(new Device(index, serial)) { }
    
    Device& operator*() { get()->load(); return std::shared_ptr<Device>::operator*(); }
    Device const& operator*() const { get()->load(); return std::shared_ptr<Device>::operator*(); }
    Device* operator->() { get()->load(); return std::shared_ptr<Device>::operator->(); }
    Device const* operator->() const { get()->load(); return std::shared_ptr<Device>::operator->(); }
  };

  // freenect callbacks go here since they need complete definition of Device
  void DepthStream::freenectCallback(freenect_device* fn_device, void* depth, uint32_t timestamp) {
    Device* device = static_cast<Device*>(freenect_get_user(fn_device));
    device->depth->publish(Frame(freenect_get_current_depth_mode(fn_device), timestamp, static_cast<uint16_t*>(depth)));
    device->depth->verifySlots(); // stop if unused
  }
  void VideoStream::freenectCallback(freenect_device* fn_device, void* color, uint32_t timestamp) {
    Device* device = static_cast<Device*>(freenect_get_user(fn_device));
    device->video->publish(Frame(freenect_get_current_video_mode(fn_device), timestamp, static_cast<uint8_t*>(color)));
    device->video->verifySlots(); // stop if unused
  }
  
  // driver starts here
  namespace {
    /// Make sure threads join when going out of scope
    struct ThreadGuard {
      void operator()(std::thread* thread) const {
        if (thread->joinable())
          thread->join();
        delete thread;
      }
    };
    std::map<std::string, device_ptr> stored_devices; // MUST be declared before event_thread
    std::unique_ptr<std::thread, ThreadGuard> event_thread = nullptr;
    std::mutex event_thread_mutex;
    timeval noblock = {0, 0};
    bool running = true;
    
    struct sigaction signal_handler;
    /// Shutdown gracefully on SIGINT
    void handleSignal(int signal) {
      std::cout << "caught signal " << signal << std::endl;
      running = false;
      event_thread.reset();
      exit(1);
    }
    
    /// Main event loop 
    void run() {
      while (running) {
        event_thread_mutex.lock();
        
        for (std::map<std::string, device_ptr>::iterator iter = stored_devices.begin(); iter != stored_devices.end();) {
          device_ptr& device = iter->second;
          if (device.unique()) // close unused devices
            iter = stored_devices.erase(iter);
          else
            ++iter;
        }
        
        if (stored_devices.empty())
          running = false;

        try
        {
          int ret = freenect_process_events_timeout(fn_context, &noblock);
          if (ret < 0)
            throw new freenect_exception(ret, "freenect_process_events_timeout");
        } catch (freenect_exception ex) {
          std::cout << ex.what() << std::endl;
        }
        
        event_thread_mutex.unlock();
      }
      
      event_thread_mutex.lock();
      try
      {
        int ret = freenect_shutdown(fn_context);
        if (ret < 0)
          throw new freenect_exception(ret, "freenect_shutdown");
        fn_context = nullptr;
      } catch (freenect_exception ex) {
        std::cout << ex.what() << std::endl;
      }
      event_thread_mutex.unlock();
    }
  }

  /// The devices() method provides a static entry point for applications.
  /// @return A vector of connected devices, no larger than max_devices
  /// @param max_devices Maximum number of devices to open
  /// @throw freenect_exception A call to a freenect function returned an error
  std::vector<device_ptr> devices(unsigned int max_devices = UINT_MAX) {
    // set up SIGINT handler to shutdown gracefully on Ctrl-c
    signal_handler.sa_handler = handleSignal;
    sigemptyset(& signal_handler.sa_mask);
    signal_handler.sa_flags = 0;
    sigaction(SIGINT, &signal_handler, NULL);
    
    event_thread_mutex.lock();
    if (not fn_context) {
      int ret = freenect_init(&fn_context, nullptr);
      if (ret < 0)
        throw freenect_exception(ret, "freenect_init");
    }

    freenect_device_attributes* attributes;
    int num_devices = freenect_list_device_attributes(fn_context, &attributes);
    if (num_devices < 0) {
      freenect_free_device_attributes(attributes);
      throw freenect_exception(num_devices, "freenect_list_device_attributes");
    }

    std::vector<unsigned int> k4w_indexes;
    // open Kinect(s) and take note of K4W(s)
    for (unsigned int index = 0; index < num_devices; ++index, attributes = attributes->next) {
      if (stored_devices.size() >= max_devices)
        break;
      
      std::cout << "camera serial = " << attributes->camera_serial << std::endl;
      
      if (strcmp(attributes->camera_serial, "0000000000000000") == 0) { // K4W has null serial
        k4w_indexes.push_back(index);
        continue;
      }
      
      if (! stored_devices.count(attributes->camera_serial))
        stored_devices.insert(std::make_pair(attributes->camera_serial, device_ptr(index, attributes->camera_serial)));
    }
    // open K4W(s) - they go in stored_devices with keys matching their indices
    for (unsigned int index : k4w_indexes) {
      if (stored_devices.size() >= max_devices)
        break;
      
      if (! stored_devices.count(std::to_string(index)))
        stored_devices.insert(std::make_pair(std::to_string(index), device_ptr(index, "0000000000000000")));
    }
    
    std::vector<device_ptr> devices;
    for (std::pair<std::string, device_ptr> device : stored_devices) {
      if (devices.size() < max_devices)
        devices.push_back(device.second);
    }
    
    event_thread_mutex.unlock();
    freenect_free_device_attributes(attributes);
    
    if (! (event_thread && event_thread->joinable()))
      event_thread.reset(new std::thread(&run));
    
    return devices;
  }
}
