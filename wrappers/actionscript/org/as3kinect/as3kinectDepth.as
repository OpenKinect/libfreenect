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
	
	import flash.utils.ByteArray;
	import flash.display.BitmapData;
	
	public class as3kinectDepth {
		private var _socket:as3kinectSocket;
		private var _data:ByteArray;
		private var _depth_busy:Boolean;
		private var _is_mirrored:Boolean;
		private var _min_distance:int;
		private var _max_distance:int;
		private var _compression:int;
		public var bitmap:BitmapData;

		public function as3kinectDepth(){
			_socket = as3kinectSocket.instance;
			_data = new ByteArray;
			_depth_busy = false;
			_is_mirrored = false;
			_min_distance = 600;
			_max_distance = 800;
			_compression = 20;
			bitmap = new BitmapData(as3kinect.IMG_WIDTH, as3kinect.IMG_HEIGHT, false, 0xFF000000);
		}

		/*
		 * Tell server to send the latest depth frame
		 * Note: We should lock the command while we are waiting for the data to avoid lag
		 */
		public function getBuffer():void {
			if(!_depth_busy){
				_depth_busy = true;
				_data.clear();
				_data.writeByte(as3kinect.CAMERA_ID);
				_data.writeByte(as3kinect.GET_DEPTH);
				_data.writeInt(0);
				if(_socket.sendCommand(_data) != as3kinect.SUCCESS){
					throw new Error('Data was not complete');
				}
			}
		}
		
		/*
		 * Tell server to send the latest depth raw frame
		 * Note: We should lock the command while we are waiting for the data to avoid lag
		 */
		public function getRawBuffer():void {
			if(!_depth_busy){
				_depth_busy = true;
				_data.clear();
				_data.writeByte(as3kinect.CAMERA_ID);
				_data.writeByte(as3kinect.GET_RAW_DEPTH);
				_data.writeInt(0);
				if(_socket.sendCommand(_data) != as3kinect.SUCCESS){
					throw new Error('Data was not complete');
				}
			}
		}
		
		public function set busy(flag:Boolean):void 
		{
			_depth_busy = flag;
		}
		
		public function get busy():Boolean 
		{
			return _depth_busy;
		}

		public function set mirrored(flag:Boolean):void 
		{
			_data.clear();
			_data.writeByte(as3kinect.CAMERA_ID);
			_data.writeByte(as3kinect.MIRROR_DEPTH);
			_data.writeInt(int(flag));
			if(_socket.sendCommand(_data) != as3kinect.SUCCESS){
				throw new Error('Depth: Cannot change mirror state');
			} else {
				_is_mirrored = flag;
			}
		}
		
		public function get mirrored():Boolean 
		{
			return _is_mirrored;
		}
		
		public function set minDistance(distance:int):void 
		{
			_data.clear();
			_data.writeByte(as3kinect.CAMERA_ID);
			_data.writeByte(as3kinect.MIN_DEPTH);
			_data.writeInt(int(distance));
			if(_socket.sendCommand(_data) != as3kinect.SUCCESS){
				throw new Error('Depth: Cannot change mirror state');
			} else {
				_min_distance = distance;
			}
		}
		
		public function get minDistance():int 
		{
			return _min_distance;
		}
		
		public function set maxDistance(distance:int):void 
		{
			_data.clear();
			_data.writeByte(as3kinect.CAMERA_ID);
			_data.writeByte(as3kinect.MAX_DEPTH);
			_data.writeInt(int(distance));
			if(_socket.sendCommand(_data) != as3kinect.SUCCESS){
				throw new Error('Depth: Cannot change mirror state');
			} else {
				_max_distance = distance;
			}
		}
		
		public function get maxDistance():int 
		{
			return _max_distance;
		}
		
		public function set compression(quality:int):void 
		{
			_data.clear();
			_data.writeByte(as3kinect.CAMERA_ID);
			_data.writeByte(as3kinect.DEPTH_COMPRESSION);
			_data.writeInt(int(quality));
			if(_socket.sendCommand(_data) != as3kinect.SUCCESS){
				throw new Error('Depth: Cannot change depth compression');
			} else {
				_compression = quality;
			}
		}
		
		public function get compression():int 
		{
			return _compression;
		}
	}
}
