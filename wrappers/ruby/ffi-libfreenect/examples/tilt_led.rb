class TILT_LED

  def initialize
    $: << File.expand_path(File.join(File.dirname(__FILE__), '../lib'))
    require 'freenect'
    check_args
    initialize_device
    tilt_led_set
    cleanup
  end

  #
  # Define usage
  #
  def script_usage
    puts "Usage: ruby tilt_led.rb -l (0-6) -t (-30-30)"
    puts "LED Options:"
    puts "   0 - Off"
    puts "   1 - Green"
    puts "   2 - Red"
    puts "   3 - Yellow"
    puts "   4 - Blink Yellow"
    puts "   5 - Blink Green"
    puts "   6 - Blink Red/Yellow"
    puts "Tilt Options:"
    puts "   Value must be between -30 and 30 degrees."
    exit 1
  end

  #
  # Check arguments. I'm sure there's a better way to do this. What can I saw, I'm a noob.
  #
  def check_args
    if( ARGV.include?("-l") ||  ARGV.include?("-t") )
      if (ARGV.include?("-l"))
        @led_option = ARGV[(ARGV.index("-l") + 1 )]
        ARGV.delete("-l")
        ARGV.delete(@led_option)
        if @led_option.to_i < 0 || @led_option.to_i > 6
          script_usage
        end
      end
      if (ARGV.include?("-t"))
        @tilt_option = ARGV[(ARGV.index("-t") + 1 )]
        ARGV.delete("-t")
        ARGV.delete(@tilt_option)
        if @tilt_option.to_i < -30 || @tilt_option.to_i > 30
          script_usage
        end
      end
    else
      script_usage
    end
  end
  
  def initialize_device
    @ctx = FFI::MemoryPointer.new(:pointer)
    @dev = FFI::MemoryPointer.new(:pointer)
    #
    # initialize context / @device
    #
    if (FFI::Freenect.freenect_init(@ctx,nil) != 0)
        puts "Error: can't initialize context"
        exit 1
    end
    @ctx = @ctx.read_pointer

    if (FFI::Freenect.freenect_open_device(@ctx, @dev, 0) != 0)
      puts "Error: can't initialize device"
      exit 1
    end
    @dev = @dev.read_pointer
  end
  
  def tilt_led_set
    #
    # Set tilt and/or led options on the @device
    #
    puts "Number of devices: #{FFI::Freenect.freenect_num_devices(@ctx)}"
    if (not @tilt_option.nil?)
      puts "Tilt set to: #{@tilt_option}"
      FFI::Freenect.freenect_set_tilt_degs(@dev, @tilt_option.to_f)
    end
    if (not @led_option.nil?)
      puts "LED set to: #{@led_option}"
      FFI::Freenect.freenect_set_led(@dev, @led_option.to_i)
    end
  end

  def cleanup
    #
    # Close the context and @device (for cleanup purposes)
    #
    FFI::Freenect.freenect_close_device(@dev)
    if FFI::Freenect.freenect_shutdown(@ctx) != 0
      puts "Error shutting down context"
      exit 1
    else
      puts "Successfully shut down context"
      exit 0
    end
  end
end

run = TILT_LED.new