#!/usr/bin/env ruby
# Actual video and depth capture work similarly to eachother.
# But they're still both pretty un-sugary.
#
# Future versions will probably abstract this and try to make it more 
# ruby-ish.
#
# The example below shows how to capture a single video frame to a PPM file.

$: << File.expand_path(File.join(File.dirname(__FILE__), "../lib"))
require 'freenect'
ctx = Freenect.init()

devs = ctx.num_devices

STDERR.puts "Number of Kinects detected: #{devs}"
unless devs > 0
  STDERR.puts "Error: no kinect detected"
  exit 1
end

STDERR.puts "Selecting device 0"
dev = ctx.open_device(0)

dev.set_led(:green)   # play with the led

dev.set_video_format(Freenect::VIDEO_RGB)
dev.start_depth()
dev.start_video()

$snapshot_finished = nil

STDERR.puts "Attempting snapshot"
dev.set_video_callback do |device, video, timestamp|
  if not $snapshot_finished
    fname = "%i.ppm" % timestamp
    STDERR.puts "Writing #{fname}"
    File.open(fname, "w") do |f|
      f.puts("P6 %d %d 255\n" % [ Freenect::FRAME_W, Freenect::FRAME_H ] )
      f.write(video.read_string_length(Freenect::RGB_SIZE))
    end
    $snapshot_finished = true
  end
end

ret = -1
until $snapshot_finished 
  break if (ret=ctx.process_events) < 0
end

if ret < 0
  STDERR.puts "Error: unable to take snapshot. process_events code=#{ret}"
end

dev.set_led(:off)
dev.stop_depth
dev.stop_video
dev.close
ctx.close


