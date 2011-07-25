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
	 
 	import flash.utils.ByteArray;
	
	public class as3kinect {

		public static const SUCCESS:int = 0;
		public static const ERROR:int = -1;

		public static const SERVER_IP:String = "localhost";
		public static const SOCKET_PORT:int = 6001;

		public static const CAMERA_ID:int = 0;
		public static const MOTOR_ID:int = 1;
		public static const MIC_ID:int = 2;
		
		public static const GET_DEPTH:int = 0;
		public static const GET_RAW_DEPTH:int = 1;
		public static const GET_VIDEO:int = 2;
		public static const MIRROR_DEPTH:int = 3;
		public static const MIRROR_VIDEO:int = 4;
		public static const MIN_DEPTH:int = 5;
		public static const MAX_DEPTH:int = 6;
		public static const DEPTH_COMPRESSION:int = 7;
		public static const VIDEO_COMPRESSION:int = 8;
		
		public static const MOVE_MOTOR:int = 0;
		public static const LED_COLOR:int = 1;
		public static const ACCEL_DATA:int = 2;
		
		public static const IMG_WIDTH:int = 640;
		public static const IMG_HEIGHT:int = 480;

		public static const RAW_IMG_SIZE:int = IMG_WIDTH * IMG_HEIGHT * 4;
		public static const DATA_IN_SIZE:int = 3 * 2 + 3 * 8;
		public static const COMMAND_SIZE:int = 6;
		
		public static const MAX_BLOBS:int = 15;
		public static const BLOB_MASK:uint = 0xFFFFFFFF;
		public static const BLOB_COLOR:uint = 0xFFFFFFFF;
		public static const BLOB_FILL_COLOR:uint = 0xFF0000FF;
		public static const BLOB_PROCESSED_COLOR:uint = 0x00FF00FF;
		public static const BLOB_MIN_WIDTH:uint = 15;
		public static const BLOB_MAX_WIDTH:uint = 30;
		public static const BLOB_MIN_HEIGHT:uint = 15;
		public static const BLOB_MAX_HEIGHT:uint = 80;
		
		public static const HORIZONTAL_BLOBS_SEARCH_STEP_WIDTH : int = 3;
        public static const HORIZONTAL_BLOBS_SEARCH_SENSIVITY : int = 15;
        public static const HORIZONTAL_BLOBS_HOLD_TOLERANCE : int=100;
	}
}
