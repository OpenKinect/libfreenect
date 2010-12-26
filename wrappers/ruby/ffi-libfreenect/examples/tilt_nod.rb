#!/usr/bin/env ruby
#

$: << File.expand_path(File.join(File.dirname(__FILE__), "../lib"))

require 'freenect'

ctx = Freenect.init
unless ctx.num_devices() > 0
  STDERR.puts "No kinect device detected"
  exit 1
end

dev = ctx[0]
dev.set_led(:blink_red_yellow)

3.times do
  dev.set_tilt_degrees(15)
  sleep 2
  dev.set_tilt_degrees(-15)
  sleep 2
end
dev.set_tilt_degrees(0.0)

dev.set_led(:off)
dev.close
ctx.shutdown

