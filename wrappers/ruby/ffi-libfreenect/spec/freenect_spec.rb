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
      mode = Freenect.video_mode(:medium, :rgb)
      mode.resolution.should == :medium
      mode.format.should == :rgb
    end

    it "should enumerate the depth modes supported by the driver" do
      modes = Freenect.depth_modes
      modes.should be_kind_of(Array)
      modes.length.should be > 0
      modes.each {|x| x.should be_kind_of(Freenect::FrameMode)}
      modes.each {|x| x.frame_mode_type.should == :depth}
    end
    it "should be able to find basic depth modes" do
      mode = Freenect.depth_mode(:medium, :depth_11bit)
      mode.resolution.should == :medium
      mode.format.should == :depth_11bit
    end
  end
end

