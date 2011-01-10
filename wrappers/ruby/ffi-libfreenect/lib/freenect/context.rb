require 'ffi/freenect'
require 'freenect/device'

module Freenect
  class ContextError < StandardError
  end

  class Context
    def initialize(usb_ctx=nil)
      ctx_p = FFI::MemoryPointer.new(:pointer)
      if ::FFI::Freenect.freenect_init(ctx_p, usb_ctx) != 0
        raise ContextError, "freenect_init() returned nonzero"
      elsif ctx_p.null?
        raise ContextError, "freenect_init() produced a NULL context"
      end
      @ctx = ctx_p.read_pointer
    end

    def context
      if @ctx_closed
        raise ContextError, "This context has been shut down and can no longer be used"
      else
        return @ctx
      end
    end

    def num_devices
      ::FFI::Freenect.freenect_num_devices(self.context)
    end

    def open_device(idx)
      return Device.new(self, idx)
    end

    alias [] open_device

    def set_log_level(loglevel)
      ::FFI::Freenect.freenect_set_log_level(self.context, loglevel)
    end

    alias log_level= set_log_level

    def set_log_callback(&block)
      ::FFI::Freenect.freenect_set_log_callback(self.context, block)
    end

    def process_events
      ::FFI::Freenect.freenect_process_events(self.context)
    end

    def close
      unless closed?
        if ::FFI::Freenect.freenect_shutdown(@ctx) != 0
          raise ContextError, "freenect_shutdown() returned nonzero"
        end
        @ctx_closed = true
      end
    end

    alias shutdown close

    def closed?
      @ctx_closed == true
    end
  end
end
