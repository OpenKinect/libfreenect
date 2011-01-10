
require 'freenect'

module Freenect
  module Sync
    class FormatError < StandardError
    end

    # Synchronous video function (starts the runloop if it isn't running)
    #
    # @param idx 
    #   Device index. Default: 0
    #
    # @param fmt
    #   Video Format. See FFI::Freenect::VIDEO_FORMATS. Default is :rgb
    #
    # @return [Numeric, String]
    #   Returns an array containing a numeric timestamp and the video buffer 
    #   snapshot with a size based on the requested video format.
    #
    # @raise FormatError
    #   An exception is raised if an invalid format is specified.
    #
    # @raise RuntimeError
    #   An exception is raised if an unknown error occurs in the 
    #   freenect_sync_get_video function
    #   
    def self.get_video(idx=nil, fmt=nil)
      idx ||= 0
      fmt ||= :rgb

      if (buf_size = Freenect.lookup_video_size(fmt)).nil?
        raise(FormatError, "Invalid video format: #{fmt.inspect}")
      end

      video_p = FFI::MemoryPointer.new(buf_size)
      timestamp_p = FFI::MemoryPointer.new(:uint32)

      ret = ::FFI::Freenect.freenect_sync_get_video(video_p, timestamp_p, idx, Freenect.lookup_video_format(fmt))

      if ret != 0
        raise("Unknown error in freenect_sync_get_video()") # TODO is errno set or something here?
      else
        return [timestamp_p.read_int, video_p.read_string_length(buf_size)]
      end
    end

    # Synchronous depth function (starts the runloop if it isn't running)
    #
    # @param idx 
    #   Device index. Default: 0
    #
    # @param fmt
    #   Video Format. See FFI::Freenect::DEPTH_FORMATS. Default is :rgb
    #
    # @return [Numeric, String]
    #   Returns an array containing a numeric timestamp and the depth buffer 
    #   snapshot with a size based on the requested video format.
    #
    # @raise FormatError
    #   An exception is raised if an invalid format is specified.
    #
    # @raise RuntimeError
    #   An exception is raised if an unknown error occurs in the 
    #   freenect_sync_get_video function
    #   
    def self.get_depth(idx=0, fmt=:depth_11bit)
      idx ||= 0
      fmt ||= :depth_11bit

      if (buf_size = Freenect.lookup_depth_size(fmt)).nil?
        raise(FormatError, "Invalid depth format: #{fmt.inspect}")
      end

      depth_p = FFI::MemoryPointer.new(buf_size)
      timestamp_p = FFI::MemoryPointer.new(:uint32)

      ret = ::FFI::Freenect.freenect_sync_get_depth(depth_p, timestamp_p, idx, Freenect.lookup_depth_format(fmt))
      if ret != 0
        raise("Unknown error in freenect_sync_get_depth()") # TODO is errno set or something here?
      else
        return [timestamp_p.read_int, video_p.read_string_length(buf_size)]
      end
    end

    # Stops the sync runloop
    def self.stop
      FFI::Freenect.freenect_sync_stop()
    end

  end
end

