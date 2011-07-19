/*
 * 
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 * 
 * Copyright (c) 2010 individual OpenKinect contributors. See the CONTRIB file 
 * for details.
 * 
 * This code is licensed to you under the terms of the Apache License, version 
 * 2.0, or, at your option, the terms of the GNU General Public License, 
 * version 2.0. See the APACHE20 and GPL20 files for the text of the licenses, 
 * or the following URLs:
 * http://www.apache.org/licenses/LICENSE-2.0
 * http://www.gnu.org/licenses/gpl-2.0.txt
 * 
 * If you redistribute this file in source form, modified or unmodified, 
 * you may:
 * 1) Leave this header intact and distribute it under the same terms,
 * accompanying it with the APACHE20 and GPL20 files, or
 * 2) Delete the Apache 2.0 clause and accompany it with the GPL20 file, or
 * 3) Delete the GPL v2.0 clause and accompany it with the APACHE20 file
 * In all cases you must keep the copyright notice intact and include a copy 
 * of the CONTRIB file.
 * Binary distributions must follow the binary distribution requirements of 
 * either License.
 * 
 */

package org.as3kinect {
	
	import org.as3kinect.as3kinect;
	import org.as3kinect.as3kinectSocket;
	import org.as3kinect.objects.motorData;
	
	import flash.utils.ByteArray;
	
	public class as3kinectMotor {
		private var _socket:as3kinectSocket;
		private var _data:ByteArray;
		private var _motor_busy:Boolean;
		private var _motor_data:motorData;
		private var _motor_position:Number;

		public function as3kinectMotor(){
			_socket = as3kinectSocket.instance;
			_data = new ByteArray;
			_motor_busy = false;
			_motor_data = new motorData;
			_motor_position = 0;
		}


		/*
		 * Tell server to send the latest depth frame
		 * Note: We should lock the command while we are waiting for the data to avoid lag
		 */
		public function getData():void {
			if(!_motor_busy){
				_motor_busy = true;
				_data.clear();
				_data.writeByte(as3kinect.MOTOR_ID);
				_data.writeByte(as3kinect.ACCEL_DATA);
				_data.writeInt(0);
				if(_socket.sendCommand(_data) != as3kinect.SUCCESS){
					throw new Error('Data was not complete');
				}
			}
		}
		
		/*
		 * Set motor data from ByteArray
		 */
		public function updateDataFromBytes(bytes:ByteArray):void{
			_motor_data.ax = bytes.readShort();
			_motor_data.ay = bytes.readShort();
			_motor_data.az = bytes.readShort();
			_motor_data.dx = bytes.readDouble();
			_motor_data.dy = bytes.readDouble();
			_motor_data.dz = bytes.readDouble();
		}
		
		public function get data():motorData 
		{
			return _motor_data;
		}
		
		public function set data(data:motorData):void 
		{
			_motor_data = data;
		}
		
		public function set busy(flag:Boolean):void 
		{
			_motor_busy = flag;
		}
		
		public function get busy():Boolean 
		{
			return _motor_busy;
		}

		public function set position(position:Number):void 
		{
			_data.clear();
			_data.writeByte(as3kinect.MOTOR_ID);
			_data.writeByte(as3kinect.MOVE_MOTOR);
			_data.writeInt(position);
			if(_socket.sendCommand(_data) != as3kinect.SUCCESS){
				throw new Error('Data was not complete');
			} else {
				_motor_position = position;
			}
		}

		public function get position():Number
		{
			return _motor_position;
		}

		// 0 = Turn Off
		// 1 = Green
		// 2 = Red
		// 3 = Orange
		// 4 = Blink Green-Off
		// 6 = Blink Red-Orange
		public function set ledColor(color:Number):void 
		{
			_data.clear();
			_data.writeByte(as3kinect.MOTOR_ID);
			_data.writeByte(as3kinect.LED_COLOR);
			_data.writeInt(color);
			if(_socket.sendCommand(_data) != as3kinect.SUCCESS){
				throw new Error('Data was not complete');
			}
		}
	}
}
