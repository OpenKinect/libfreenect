#!/usr/bin/env ruby
# This is a more or less a straight ruby port of the "record" utility 
# included in the libfreenect fakenect directory using the 
# ffi-libfreenect ruby class wrappers. 
#
# This was really implemented just to see if ffi-libfreenect was working.
# However, the output should be completely compatible the C version fakenect.
#
# usage: record.rb output_dir
#

begin
  require 'rubygems'
rescue LoadError
end

$: << File.expand_path(File.join(File.dirname(__FILE__), "../lib"))
require 'freenect'

$last_timestamp = 0
$record_running = true

def open_dump(type, timestamp, extension)
  $last_timestamp = timestamp
  filename = "%s-%f-%u.%s" % [ type, Time.now.to_f, timestamp, extension]
  STDERR.puts "Writing: #{File.join($out_dir, filename)}"
  File.open(File.join($out_dir,"INDEX.txt"), "a"){|f| f.puts(filename) }
  File.open(File.join($out_dir, filename), "wb") {|f| yield f}
end

orig_dir = Dir.pwd
unless $out_dir = ARGV.shift
  STDERR.puts "usage: #{File.basename $0} output_dir"
  exit 1
end
Dir.mkdir($out_dir) unless File.directory?($out_dir)

trap('INT') do
  STDERR.puts "Caught INT signal cleaning up"
  $record_running = false
end

ctx = Freenect.init()
dev = ctx.open_device(0)

dev.depth_mode = Freenect.depth_mode(:medium, :depth_11bit)
dev.video_mode = Freenect.video_mode(:medium, :rgb)
dev.start_depth()
dev.start_video()

dev.set_depth_callback do |device, depth, timestamp|
  open_dump('d', timestamp, "pgm") do |f|
    f.puts("P5 %d %d 65535\n" % [ dev.depth_mode.width, dev.depth_mode.height ] )
    f.write(depth.read_string_length(dev.depth_mode.bytes))
  end
end

dev.set_video_callback do |device, video, timestamp|
  open_dump('r', timestamp, 'ppm') do |f|
    f.puts("P6 %d %d 255\n" % [ dev.video_mode.width, dev.video_mode.height ] )
    f.write(video.read_string_length(dev.video_mode.bytes))
  end
end

while $record_running and (ctx.process_events >= 0)
  open_dump('a', $last_timestamp, "dump") do |f|
    state = dev.get_tilt_state
    f.write(state.to_ptr.read_string_length(state.size))
  end
end

Dir.chdir(orig_dir)
dev.stop_depth
dev.stop_video
dev.close
ctx.close
