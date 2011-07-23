
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

  # def self.lookup_video_format(fmt)
  #   if fmt.is_a?(Numeric)
  #     unless (p=::FFI::Freenect.freenect_get_video_mode(self.device)).null?
  #       frame_mode = FrameMode.new(p)
  #     else
  #       raise DeviceError, "freenect_get_video_mode() returned a NULL frame_mode"
  #     end
  #   else
  #     unless (p=::FFI::Freenect.freenect_get_video_mode(self.device)).null?
  #       frame_mode = FrameMode.new(p)
  #     else
  #       raise DeviceError, "freenect_get_video_mode() returned a NULL frame_mode"
  #     end
  #   end
  #   
  #   return frame_mode[:format][:video_format]
  # end
  # 
  # def self.lookup_video_size(fmt)
  #   l_fmt = (fmt.is_a?(Numeric) ? FFI::Freenect::VIDEO_FORMATS[fmt] : fmt)
  #   if l_fmt.nil? or (sz = FFI::Freenect::VIDEO_SIZES[l_fmt]).nil?
  #     return nil
  #   else
  #     return sz
  #   end
  # end
  # 
  # def self.lookup_depth_format(fmt)
  #   return (fmt.is_a?(Numeric) ? fmt : FFI::Freenect::DEPTH_FORMATS[fmt])
  # end
  # 
  # def self.lookup_depth_size(fmt)
  #   l_fmt = (fmt.is_a?(Numeric) ? FFI::Freenect::DEPTH_FORMATS[fmt] : fmt)
  #   if l_fmt.nil? or (sz = FFI::Freenect::DEPTH_SIZES[l_fmt]).nil?
  #     return nil
  #   else
  #     return sz
  #   end
  # end
end
