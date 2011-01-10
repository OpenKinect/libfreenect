
# we may one day have a native extension for bindings... for now only 
# ffi/freenect exists
require 'ffi/freenect'
require 'freenect/context'
require 'freenect/device'

module Freenect
  include FFI::Freenect

  def self.init(*args)
    Context.new(*args)
  end

  def self.lookup_video_format(fmt)
    return (fmt.is_a?(Numeric) ? fmt : FFI::Freenect::VIDEO_FORMATS[fmt])
  end

  def self.lookup_video_size(fmt)
    l_fmt = (fmt.is_a?(Numeric) ? FFI::Freenect::VIDEO_FORMATS[fmt] : fmt)
    if l_fmt.nil? or (sz = FFI::Freenect::VIDEO_SIZES[l_fmt]).nil?
      return nil
    else
      return sz
    end
  end

  def self.lookup_depth_format(fmt)
    return (fmt.is_a?(Numeric) ? fmt : FFI::Freenect::DEPTH_FORMATS[fmt])
  end

  def self.lookup_depth_size(fmt)
    l_fmt = (fmt.is_a?(Numeric) ? FFI::Freenect::DEPTH_FORMATS[fmt] : fmt)
    if l_fmt.nil? or (sz = FFI::Freenect::DEPTH_SIZES[l_fmt]).nil?
      return nil
    else
      return sz
    end
  end
end
