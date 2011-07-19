package org.as3kinect.objects {
    /**
     * @author stevie
     * @date 29.06.2011
     */
    public class as3kinectCalibrationParams {
        public var screenHeight : int = 0;
        public var screenWidth : int = 0;
        public var xOffsetTop : Number;
        public var xOffsetDifference : Number;
        public var xScreenFactorTop : Number;
        public var xScreenFactorDifference : Number;
        public var yOffset : Number;
        public var yScreenFactor : Number;
        public var sliceRectY : Number = 0;
        public var sliceRectHeight : Number = 20;
        public var calibrationComplete : Boolean;
		
		public function toString() : String{
			var str : String = "screenHeight: " + screenHeight + "\n";
			str += "screenHeight: " + screenHeight + "\n";       
			str += "screenWidth: " + screenWidth + "\n";       
			str += "xOffsetTop: " + xOffsetTop + "\n";       
			str += "xOffsetDifference: " + xOffsetDifference + "\n";       
			str += "xScreenFactorTop: " + xScreenFactorTop + "\n";       
			str += "xScreenFactorDifference: " + xScreenFactorDifference + "\n";       
			str += "yOffset: " + yOffset + "\n";       
			str += "yScreenFactor: " + yScreenFactor + "\n";       
			str += "sliceRectY: " + sliceRectY + "\n";       
			str += "slicsliceRectHeighteRectY: " + sliceRectHeight + "\n";    
			
			return str;   
		}
    }
}
