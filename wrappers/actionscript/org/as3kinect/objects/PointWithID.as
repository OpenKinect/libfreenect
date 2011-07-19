package org.as3kinect.objects {
    import flash.geom.Point;

    /**
     * @author stevie
     * @date 24.06.2011
     */
    public class PointWithID extends Point {
    	public var id:int;
    	public var inUse:Boolean;
        public function PointWithID(x : Number = 0, y : Number = 0) {
            super(x, y);
        }
    }
}
