
# we may one day have a native extension for bindings... for now only 
# ffi/freenect exists
require 'ffi/freenect'
require 'freenect/context'
require 'freenect/device'

module Freenect
  include FFI::Freenect
  FrameMode = FFI::Freenect::FrameMode

  def self.init(*args)
    Context.new(*args)
  end

  def self.num_video_modes
    ::FFI::Freenect.freenect_get_video_mode_count()
  end
  def self.video_mode(a,b=nil)
    x = if a.is_a?(Numeric)
      ::FFI::Freenect.freenect_get_video_mode(a)
    else
      ::FFI::Freenect.freenect_find_video_mode(a,b)
    end
    x.frame_mode_type = :video unless x.nil?
    x
  end
  def self.video_modes
    (0...self.num_video_modes).map do |ii|
      self.video_mode(ii)
    end
  end
  
  def self.num_depth_modes
    ::FFI::Freenect.freenect_get_depth_mode_count()
  end
  def self.depth_mode(a,b=nil)
    x = if a.is_a?(Numeric)
      ::FFI::Freenect.freenect_get_depth_mode(a)
    else
      ::FFI::Freenect.freenect_find_depth_mode(a,b)
    end
    x.frame_mode_type = :depth unless x.nil?
    x
  end
  def self.depth_modes
    (0...self.num_depth_modes).map do |ii|
      self.depth_mode(ii)
    end
  end
end
