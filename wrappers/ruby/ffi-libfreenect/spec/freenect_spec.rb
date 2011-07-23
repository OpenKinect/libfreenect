require 'spec_helper'

describe Freenect do
  context "singleton methods" do

    it "should create a new context" do
      ctx = Freenect.init()
      ctx.should be_kind_of(Freenect::Context)
      lambda { ctx.close }.should_not raise_error
    end

    it "should enumerate the video modes supported by the driver" do
      modes = Freenect.video_modes
      modes.should be_kind_of(Array)
      modes.length.should be > 0
      modes.each {|x| x.should be_kind_of(Freenect::FrameMode)}
      modes.each {|x| x.frame_mode_type.should == :video}
    end
    it "should be able to find basic video modes" do
      [
        [:high,:rgb], [:medium,:rgb],
        [:high,:bayer], [:medium,:bayer],
        [:high,:ir_8bit], [:medium,:ir_8bit],
        [:high,:ir_10bit], [:medium,:ir_10bit],
        [:high,:ir_10bit_packed], [:medium,:ir_10bit_packed],
      ].each do |res,mode|
        x = Freenect.video_mode(res, mode)
        x.resolution.should == res
        x.format.should == mode
      end      
    end
    it "should find the right sizes for video" do
      high = Freenect.video_mode(:high, :rgb)
      high.width.should == 1280
      high.height.should == 1024
      medium = Freenect.video_mode(:medium, :rgb)
      medium.width.should == 640
      medium.height.should == 480
      ir_8bit = Freenect.video_mode(:medium, :ir_8bit)
      ir_8bit.width.should == 640
      ir_8bit.height.should == 488
    end

    it "should enumerate the depth modes supported by the driver" do
      modes = Freenect.depth_modes
      modes.should be_kind_of(Array)
      modes.length.should be > 0
      modes.each {|x| x.should be_kind_of(Freenect::FrameMode)}
      modes.each {|x| x.frame_mode_type.should == :depth}
    end
    it "should be able to find basic depth modes" do
      [
        [:medium,:depth_11bit],
        [:medium,:depth_10bit],
        [:medium,:depth_11bit_packed],
        [:medium,:depth_10bit_packed],
      ].each do |res,mode|
        x = Freenect.depth_mode(res, mode)
        x.resolution.should == res
        x.format.should == mode
      end      
    end
  end
  it "should find the right sizes for depth" do
    depth = Freenect.depth_mode(:medium, :depth_11bit)
    depth.width.should == 640
    depth.height.should == 480
  end
  
end

