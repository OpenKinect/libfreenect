require 'spec_helper'

describe Freenect do
  context "singleton methods" do

    it "should create a new context" do
      ctx = Freenect.init()
      ctx.should be_kind_of(Freenect::Context)
      lambda { ctx.close }.should_not raise_error
    end

    it "should lookup video format values" do
      Freenect.lookup_video_format(:rgb).should == Freenect::VIDEO_RGB
      Freenect.lookup_video_format(0).should == Freenect::VIDEO_RGB


      Freenect.lookup_video_format(:bayer).should == Freenect::VIDEO_BAYER
      Freenect.lookup_video_format(1).should == Freenect::VIDEO_BAYER

      Freenect.lookup_video_format(:ir_8bit).should == Freenect::VIDEO_IR_8BIT
      Freenect.lookup_video_format(2).should == Freenect::VIDEO_IR_8BIT

      Freenect.lookup_video_format(:ir_10bit).should == Freenect::VIDEO_IR_10BIT
      Freenect.lookup_video_format(3).should == Freenect::VIDEO_IR_10BIT

      Freenect.lookup_video_format(:ir_10bit_packed).should == Freenect::VIDEO_IR_10BIT_PACKED
      Freenect.lookup_video_format(4).should == Freenect::VIDEO_IR_10BIT_PACKED

      Freenect.lookup_video_format(:yuv_rgb).should == Freenect::VIDEO_YUV_RGB
      Freenect.lookup_video_format(5).should == Freenect::VIDEO_YUV_RGB

      Freenect.lookup_video_format(:yuv_raw).should == Freenect::VIDEO_YUV_RAW
      Freenect.lookup_video_format(6).should == Freenect::VIDEO_YUV_RAW
    end

    it "should lookup video format size values" do
      Freenect.lookup_video_size(:rgb).should == Freenect::RGB_SIZE
      Freenect.lookup_video_size(0).should == Freenect::RGB_SIZE

      Freenect.lookup_video_size(:bayer).should == Freenect::BAYER_SIZE
      Freenect.lookup_video_size(1).should == Freenect::BAYER_SIZE

      Freenect.lookup_video_size(:ir_8bit).should == Freenect::IR_8BIT_SIZE
      Freenect.lookup_video_size(2).should == Freenect::IR_8BIT_SIZE

      Freenect.lookup_video_size(:ir_10bit).should == Freenect::IR_10BIT_SIZE
      Freenect.lookup_video_size(3).should == Freenect::IR_10BIT_SIZE

      Freenect.lookup_video_size(:ir_10bit_packed).should == Freenect::IR_10BIT_PACKED_SIZE
      Freenect.lookup_video_size(4).should == Freenect::IR_10BIT_PACKED_SIZE

      Freenect.lookup_video_size(:yuv_rgb).should == Freenect::YUV_RGB_SIZE
      Freenect.lookup_video_size(5).should == Freenect::YUV_RGB_SIZE

      Freenect.lookup_video_size(:yuv_raw).should == Freenect::YUV_RAW_SIZE
      Freenect.lookup_video_size(6).should == Freenect::YUV_RAW_SIZE

    end

    it "should lookup depth format values" do
      Freenect.lookup_depth_format(:depth_11bit).should == Freenect::DEPTH_11BIT
      Freenect.lookup_depth_format(0).should == Freenect::DEPTH_11BIT

      Freenect.lookup_depth_format(:depth_10bit).should == Freenect::DEPTH_10BIT
      Freenect.lookup_depth_format(1).should == Freenect::DEPTH_10BIT

      Freenect.lookup_depth_format(:depth_11bit_packed).should == Freenect::DEPTH_11BIT_PACKED
      Freenect.lookup_depth_format(2).should == Freenect::DEPTH_11BIT_PACKED

      Freenect.lookup_depth_format(:depth_10bit_packed).should == Freenect::DEPTH_10BIT_PACKED
      Freenect.lookup_depth_format(3).should == Freenect::DEPTH_10BIT_PACKED
    end

    it "should lookup depth format size values" do
      Freenect.lookup_depth_size(:depth_11bit).should == Freenect::DEPTH_11BIT_SIZE
      Freenect.lookup_depth_size(0).should == Freenect::DEPTH_11BIT_SIZE

      Freenect.lookup_depth_size(:depth_10bit).should == Freenect::DEPTH_10BIT_SIZE
      Freenect.lookup_depth_size(1).should == Freenect::DEPTH_10BIT_SIZE

      Freenect.lookup_depth_size(:depth_11bit_packed).should == Freenect::DEPTH_11BIT_PACKED_SIZE
      Freenect.lookup_depth_size(2).should == Freenect::DEPTH_11BIT_PACKED_SIZE

      Freenect.lookup_depth_size(:depth_10bit_packed).should == Freenect::DEPTH_10BIT_PACKED_SIZE
      Freenect.lookup_depth_size(3).should == Freenect::DEPTH_10BIT_PACKED_SIZE
    end
  end

end

