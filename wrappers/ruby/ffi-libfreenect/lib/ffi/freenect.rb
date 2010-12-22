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
 
 
  class RawTiltState < FFI::Struct
    layout :accelerometer_x,  :int16_t,
           :accelerometer_y,  :int16_t,
           :accelerometer_z,  :int16_t, 
           :tilt_angle,       :int8_t, 
           :tilt_status,      TILT_STATUS_CODES
  end

  callback :freenect_log_cb, [:freenect_context, LOGLEVELS, :string], :void
  callback :freenect_depth_cb, [:freenect_device, :pointer, :uint32], :void
  callback :freenect_video_cb, [:freenect_device, :pointer, :uint32], :void

  attach_function :freenect_set_log_level, [:freenect_context, LOGLEVELS], :void
  attach_function :freenect_set_log_callback, [:freenect_context, :freenect_log_cb], :void
  attach_function :freenect_process_events, [:freenect_context], :int
  attach_function :freenect_num_devices, [:freenect_context], :int
  attach_function :freenect_open_device, [:freenect_context, :freenect_device, :int], :int
  attach_function :freenect_close_device, [:freenect_device], :int
  attach_function :freenect_init, [:freenect_context, :freenect_usb_context], :int
  attach_function :freenect_shutdown, [:freenect_context], :int
  attach_function :freenect_set_user, [:freenect_device, :pointer], :void
  attach_function :freenect_get_user, [:freenect_device], :pointer
  attach_function :freenect_set_depth_callback, [:freenect_device, :freenect_depth_cb], :void
  attach_function :freenect_set_video_callback, [:freenect_device, :freenect_video_cb], :void  
  attach_function :freenect_set_depth_format, [:freenect_device, DEPTH_FORMATS], :int
  attach_function :freenect_set_video_format, [:freenect_device, VIDEO_FORMATS], :int
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

  attach_function :freenect_sync_get_video, [:pointer, :pointer, :int, VIDEO_FORMATS], :int
  attach_function :freenect_sync_get_depth, [:pointer, :pointer, :int, DEPTH_FORMATS], :int
  attach_function :freenect_sync_stop, [], :void

end


