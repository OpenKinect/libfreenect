package org.as3kinect.events
{
	import flash.events.Event;

    public class as3kinectCalibrationEvent extends Event
		{
		
		public static const CALIBRATION_STEP_ONE:String = "CALIBRATION_FIRST_STEP_as3kinectCalibrationEvent";
		public static const CALIBRATION_COMPLETE:String = "CALIBRATION_COMPLETE_as3kinectCalibrationEvent";
		
        public function as3kinectCalibrationEvent(type : String)
		{
			super(type);
		}
	}
}