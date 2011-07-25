begin
  require 'rubygems'
rescue LoadError
  #nop
end

require 'ffi'

module FFI::Freenect
  extend FFI::Library
  ffi_lib 'freenect', 'freenect_sync'
  
  FRAME_W = 640
  FRAME_H = 480
  FRAME_PIX = FRAME_W * FRAME_H

  IR_FRAME_W = 640
  IR_FRAME_H = 488
  IR_FRAME_PIX = FRAME_W * FRAME_H
 
  RGB_SIZE = FRAME_PIX * 3
  BAYER_SIZE = FRAME_PIX
  YUV_RGB_SIZE = RGB_SIZE
  YUV_RAW_SIZE = FRAME_PIX * 2
  IR_8BIT_SIZE = IR_FRAME_PIX
  IR_10BIT_SIZE = IR_FRAME_PIX * 2
  IR_10BIT_PACKED_SIZE = 390400

  DEPTH_11BIT_SIZE = FRAME_PIX * 2
  DEPTH_10BIT_SIZE = DEPTH_11BIT_SIZE
  DEPTH_11BIT_PACKED_SIZE = 422400
  DEPTH_10BIT_PACKED_SIZE = 384000

  COUNTS_PER_G = 819

	LED_OFF    = 0
	LED_GREEN  = 1
	LED_RED    = 2
	LED_YELLOW = 3
	LED_BLINK_YELLOW = 4
	LED_BLINK_GREEN = 5
	LED_BLINK_RED_YELLOW = 6

  LED_OPTIONS = enum( :off,               LED_OFF,
                      :green,             LED_GREEN,
                      :red,               LED_RED,
                      :yellow,            LED_YELLOW,
                      :blink_yellow,      LED_BLINK_YELLOW,
                      :blink_green,       LED_BLINK_GREEN,
                      :blink_red_yellow,  LED_BLINK_RED_YELLOW) 
 
 
	VIDEO_RGB = 0
	VIDEO_BAYER = 1
	VIDEO_IR_8BIT = 2
	VIDEO_IR_10BIT = 3
	VIDEO_IR_10BIT_PACKED = 4
	VIDEO_YUV_RGB = 5
	VIDEO_YUV_RAW = 6

  VIDEO_FORMATS = enum( :rgb,             VIDEO_RGB,
                        :bayer,           VIDEO_BAYER,
                        :ir_8bit,         VIDEO_IR_8BIT,
                        :ir_10bit,        VIDEO_IR_10BIT,
                        :yuv_rgb,         VIDEO_YUV_RGB,
                        :yuv_raw,         VIDEO_YUV_RAW,
                        :ir_10bit_packed, VIDEO_IR_10BIT_PACKED)
  
  VIDEO_SIZES = enum( :rgb,             RGB_SIZE,
                      :bayer,           BAYER_SIZE,
                      :ir_8bit,         IR_8BIT_SIZE,
                      :ir_10bit,        IR_10BIT_SIZE,
                      :yuv_rgb,         YUV_RGB_SIZE,
                      :yuv_raw,         YUV_RAW_SIZE,
                      :ir_10bit_packed, IR_10BIT_PACKED_SIZE )

  RESOLUTION_LOW = 0
  RESOLUTION_MEDIUM = 1
  RESOLUTION_HIGH = 2
  
  RESOLUTIONS = enum( :low, RESOLUTION_LOW,
                      :medium, RESOLUTION_MEDIUM,
                      :high, RESOLUTION_HIGH)
 
	DEPTH_11BIT = 0
	DEPTH_10BIT = 1
	DEPTH_11BIT_PACKED = 2
	DEPTH_10BIT_PACKED = 3

  DEPTH_FORMATS = enum( :depth_11bit,         DEPTH_11BIT,
                        :depth_10bit,         DEPTH_10BIT,
                        :depth_11bit_packed,  DEPTH_11BIT_PACKED,
                        :depth_10bit_packed,  DEPTH_10BIT_PACKED)

  DEPTH_SIZES = enum( :depth_11bit,         DEPTH_11BIT_SIZE,
                      :depth_10bit,         DEPTH_10BIT_SIZE,
                      :depth_11bit_packed,  DEPTH_11BIT_PACKED_SIZE,
                      :depth_10bit_packed,  DEPTH_10BIT_PACKED_SIZE )

	TILT_STATUS_STOPPED = 0x00
	TILT_STATUS_LIMIT = 0x01
	TILT_STATUS_MOVING = 0x04

  TILT_STATUS_CODES = enum( :stopped,  TILT_STATUS_STOPPED,
                            :limit,    TILT_STATUS_LIMIT,
                            :moving,   TILT_STATUS_MOVING)

	DEVICE_MOTOR  = 0x01
	DEVICE_CAMERA = 0x02
	DEVICE_AUDIO  = 0x04

	DEVICES = enum( :motor, DEVICE_MOTOR,
	                :camera, DEVICE_CAMERA,
	                :audio, DEVICE_AUDIO)
 
	LOG_FATAL = 0
	LOG_ERROR = 1
	LOG_WARNING = 2 
	LOG_NOTICE = 3
	LOG_INFO = 4
	LOG_DEBUG = 5
	LOG_SPEW = 6
	LOG_FLOOD = 7

  LOGLEVELS = enum( :fatal,   LOG_FATAL,
                    :error,   LOG_ERROR,
                    :warning, LOG_WARNING,
                    :notice,  LOG_NOTICE,
                    :info,    LOG_INFO,
                    :debug,   LOG_DEBUG,
                    :spew,    LOG_SPEW,
                    :flood,   LOG_FLOOD)
  
  typedef :pointer, :freenect_context
  typedef :pointer, :freenect_device
  typedef :pointer, :freenect_usb_context # actually a libusb_context
 
  class FrameModeFormat < FFI::Union
    layout :dummy, :int32,
           :video_format, VIDEO_FORMATS,
           :depth_format, DEPTH_FORMATS
  end
  class FrameMode < FFI::Struct
    layout :reserved,               :uint32,
           :resolution,             RESOLUTIONS,
           :format,                 FrameModeFormat,
           :bytes,                  :int32,
           :width,                  :int16,
           :height,                 :int16,
           :data_bits_per_pixel,    :int8,
           :padding_bits_per_pixel, :int8,
           :framerate,              :int8,
           :is_valid,               :int8
    def is_valid?
     self[:is_valid] != 0
    end
    def framerate; self[:framerate]; end
    def padding_bits_per_pixel; self[:padding_bits_per_pixel]; end
    def data_bits_per_pixel; self[:data_bits_per_pixel]; end
    def height; self[:height]; end
    def width; self[:width]; end
    def bytes; self[:bytes]; end
    def resolution; self[:resolution]; end
    def video_format; self[:format][:video_format]; end
    def depth_format; self[:format][:depth_format]; end
    attr_accessor :frame_mode_type
    def format
      case self.frame_mode_type
      when :video
        self.video_format
      when :depth
        self.depth_format
      else
        nil
      end
    end
    def to_s
      "#<Freenect::FrameMode #{format} #{resolution} res (#{width}x#{height}) @ #{framerate} Hz>"
    end
  end
 
  class RawTiltState < FFI::Struct
    layout :accelerometer_x,  :int16,
           :accelerometer_y,  :int16,
           :accelerometer_z,  :int16, 
           :tilt_angle,       :int8, 
           :tilt_status,      TILT_STATUS_CODES
    def accelerometer
      [self[:accelerometer_x], self[:accelerometer_y], self[:accelerometer_z]]
    end
    def angle; self[:tilt_angle]; end
    def status; self[:tilt_status]; end
    def to_s
      "#<Freenect::RawTiltState #{status} @ #{angle} deg, accel: (#{accelerometer.join(',')})>"
    end
  end

  callback :freenect_log_cb, [:freenect_context, LOGLEVELS, :string], :void
  callback :freenect_depth_cb, [:freenect_device, :pointer, :uint32], :void
  callback :freenect_video_cb, [:freenect_device, :pointer, :uint32], :void

  attach_function :freenect_set_log_level, [:freenect_context, LOGLEVELS], :void
  attach_function :freenect_set_log_callback, [:freenect_context, :freenect_log_cb], :void
  attach_function :freenect_process_events, [:freenect_context], :int
  attach_function :freenect_num_devices, [:freenect_context], :int
  attach_function :freenect_select_subdevices, [:freenect_context, DEVICES], :void
  attach_function :freenect_open_device, [:freenect_context, :freenect_device, :int], :int
  attach_function :freenect_close_device, [:freenect_device], :int
  attach_function :freenect_init, [:freenect_context, :freenect_usb_context], :int
  attach_function :freenect_shutdown, [:freenect_context], :int
  attach_function :freenect_set_user, [:freenect_device, :pointer], :void
  attach_function :freenect_get_user, [:freenect_device], :pointer
  attach_function :freenect_set_depth_callback, [:freenect_device, :freenect_depth_cb], :void
  attach_function :freenect_set_video_callback, [:freenect_device, :freenect_video_cb], :void  
  attach_function :freenect_set_depth_buffer, [:freenect_device, :void], :int
  attach_function :freenect_set_video_buffer, [:freenect_device, :void], :int
  attach_function :freenect_start_depth, [:freenect_device], :int
  attach_function :freenect_start_video, [:freenect_device], :int
  attach_function :freenect_stop_depth, [:freenect_device], :int
  attach_function :freenect_stop_video, [:freenect_device], :int
  attach_function :freenect_update_tilt_state, [:freenect_device], :int
  attach_function :freenect_get_tilt_state, [:freenect_device], RawTiltState
  attach_function :freenect_get_tilt_degs, [:freenect_device], :double
  attach_function :freenect_set_tilt_degs, [:freenect_device, :double], :int
  attach_function :freenect_set_led, [:freenect_device, LED_OPTIONS], :int
  attach_function :freenect_get_mks_accel, [RawTiltState, :pointer, :pointer, :pointer], :void

  attach_function :freenect_get_video_mode_count, [], :int
  attach_function :freenect_get_video_mode, [:int], FrameMode.by_value
  attach_function :freenect_get_current_video_mode, [:freenect_device], FrameMode.by_value
  attach_function :freenect_find_video_mode, [RESOLUTIONS, VIDEO_FORMATS], FrameMode.by_value
  attach_function :freenect_set_video_mode, [:freenect_device, FrameMode.by_value], :int

  attach_function :freenect_get_depth_mode_count, [], :int
  attach_function :freenect_get_depth_mode, [:int], FrameMode.by_value
  attach_function :freenect_get_current_depth_mode, [:freenect_device], FrameMode.by_value
  attach_function :freenect_find_depth_mode, [RESOLUTIONS, DEPTH_FORMATS], FrameMode.by_value
  attach_function :freenect_set_depth_mode, [:freenect_device, FrameMode.by_value], :int

  attach_function :freenect_sync_get_video, [:pointer, :pointer, :int, VIDEO_FORMATS], :int
  attach_function :freenect_sync_get_depth, [:pointer, :pointer, :int, DEPTH_FORMATS], :int
  attach_function :freenect_sync_stop, [], :void

end


