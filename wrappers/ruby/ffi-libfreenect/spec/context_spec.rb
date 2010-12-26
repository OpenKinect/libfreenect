
require 'spec_helper'

describe Freenect::Context do
  before(:all) { @ctx = Freenect::Context.new() }
  after(:all) { @ctx.close if @ctx }

  it "should initialize a context" do
    @ctx.should be_kind_of(Freenect::Context)
  end

  it "should indicate the number of devices attached" do
    @ctx.num_devices.should >= 0
  end

  it "should open a device when one is connected" do
    if @ctx.num_devices > 0
      dev = @ctx[0]
      dev.should be_kind_of(Freenect::Device)
      lambda { dev.close }.should_not raise_error
    end
  end

  it "should raise an exception when an invalid device index is opened" do
    lambda { @ctx[@ctx.num_devices + 1] }.should raise_error(StandardError)
    lambda { @ctx.open_device(@ctx.num_devices + 1) }.should raise_error(StandardError)
  end

  it "should allow a libfreenect log level to be set using symbols or constants" do
    (@ctx.log_level = :fatal).should == :fatal
    (@ctx.log_level = Freenect::LOG_FATAL).should == 0

    (@ctx.log_level = :error).should == :error
    (@ctx.log_level = Freenect::LOG_ERROR).should == 1

    (@ctx.log_level = :warning).should == :warning
    (@ctx.log_level = Freenect::LOG_WARNING).should == 2

    (@ctx.log_level = :notice).should == :notice
    (@ctx.log_level = Freenect::LOG_NOTICE).should == 3

    (@ctx.log_level = :info).should == :info
    (@ctx.log_level = Freenect::LOG_INFO).should == 4

    (@ctx.log_level = :debug).should == :debug
    (@ctx.log_level = Freenect::LOG_DEBUG).should == 5

    (@ctx.log_level = :spew).should == :spew
    (@ctx.log_level = Freenect::LOG_SPEW).should == 6

    (@ctx.log_level = :flood).should == :flood
    (@ctx.log_level = Freenect::LOG_FLOOD).should == 7
  end

  it "should allow a log callback to be set" do
    @ctx.set_log_callback {|a,b,c| p [a,b,c] }
  end


end

