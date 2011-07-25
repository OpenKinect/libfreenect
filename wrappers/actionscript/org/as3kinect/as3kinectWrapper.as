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
	import org.as3kinect.as3kinectMotor;
	import org.as3kinect.as3kinectUI;
	import org.as3kinect.events.as3kinectSocketEvent;
	import org.as3kinect.events.as3kinectWrapperEvent;
	
	import flash.utils.ByteArray;
	import flash.geom.Rectangle;
	import flash.events.EventDispatcher;
	
	public class as3kinectWrapper extends EventDispatcher {

		private var _socket			:as3kinectSocket;
		private var _data			:ByteArray;
		private var _video_waiting	:Boolean = false;
		private var user_id			:Number;
		private var _tracked_users	:Array;
		
		public var motor:as3kinectMotor;
		public var depth:as3kinectDepth;
		public var video:as3kinectVideo;
		//public var ui:as3kinectUI;

		public function as3kinectWrapper() {
			/* Init motor, depth, video and UI objects */
			motor = new as3kinectMotor;
			depth = new as3kinectDepth;
			video = new as3kinectVideo;
			//ui = new as3kinectUI(this);
			
			/* Init socket objects */
			_socket = as3kinectSocket.instance;
			_socket.connect(as3kinect.SERVER_IP, as3kinect.SOCKET_PORT);
			_socket.addEventListener(as3kinectSocketEvent.ONDATA, dataReceived);
			
			/* Init data out buffer */
			_data = new ByteArray();
		}

		/*
		 * dataReceived from socket (Protocol definition)
		 * Metadata comes in the first and second value of the data object
		 * first:
		 *	0 -> Camera data
		 * 			second:
		 *  			0 -> Depth ARGB received
		 *  			1 -> Video ARGB received
		 *	1 -> Motor data
		 *	2 -> Microphone data
		 *	3 -> Server data
		 * 			second:
		 *  			0 -> Debug info received
		 *
		 */
		private function dataReceived(event:as3kinectSocketEvent):void{
			// Send ByteArray to position 0
			event.data.buffer.position = 0;
			switch (event.data.first) {
				case 0: //Camera
					switch (event.data.second) {
						case 0: //Depth received
							dispatchEvent(new as3kinectWrapperEvent(as3kinectWrapperEvent.ON_DEPTH, event.data.buffer));
							depth.busy = false;
						break;
						case 1: //Raw Depth received
							dispatchEvent(new as3kinectWrapperEvent(as3kinectWrapperEvent.ON_RAW_DEPTH, event.data.buffer));
							depth.busy = false;
						break;
						case 2: //Video received
							dispatchEvent(new as3kinectWrapperEvent(as3kinectWrapperEvent.ON_VIDEO, event.data.buffer));
							video.busy = false;
						break;
					}
				break;
				case 1: //Motor
					switch (event.data.second) {
						case 2: //Accelerometer received
							motor.updateDataFromBytes(event.data.buffer);
							dispatchEvent(new as3kinectWrapperEvent(as3kinectWrapperEvent.ON_ACCELEROMETER, motor.data));
							motor.busy = false;
						break;
					}
				break;
				case 2: //Mic
				break;
				case 3: //Server
					switch (event.data.second) {
						case 0: //Debug received
							//if(_debugging) _console.appendText(event.data.buffer.toString());
						break;
					}
				break;
			}
			// Clear ByteArray after used
			event.data.buffer.clear();
		}
	}
	
}
