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
	import org.as3kinect.events.as3kinectSocketEvent;
	import flash.events.Event;
	import flash.events.EventDispatcher;
	import flash.events.ProgressEvent;
	import flash.events.IOErrorEvent;
	
	import flash.net.Socket;
	import flash.utils.ByteArray;
	import flash.utils.Endian;

	/**
	 * as3kinectSocket class recieves Kinect data from the as3kinect driver.
	 */
	public class as3kinectSocket extends EventDispatcher
	{
		private static var _instance:as3kinectSocket;
		private static var _singleton_lock:Boolean = false;
		
		private var _first_byte:Number;
		private var _second_byte:Number;
		private var _packet_size:Number;
		private var _socket:Socket;
		private var _buffer:ByteArray;
		private var _data_obj:Object;
		private var _port:Number;

		public function as3kinectSocket()
		{		
			if ( !_singleton_lock ) throw new Error( 'Use as3kinectSocket.instance' );
			_socket = new Socket();
			_buffer = new ByteArray();
			_data_obj = new Object();

			_socket.addEventListener(ProgressEvent.SOCKET_DATA, onSocketData);
			_socket.addEventListener(IOErrorEvent.IO_ERROR, onSocketError);
			_socket.addEventListener(Event.CONNECT, onSocketConnect);
		}
		
		public function connect(host:String = 'localhost', port:uint = 6001):void
		{
			_port = port;
			_packet_size = 0;
			if (!this.connected) 
				_socket.connect(host, port);
			else
				dispatchEvent(new as3kinectSocketEvent(as3kinectSocketEvent.ONCONNECT, null));
		}
		
		public function get connected():Boolean
		{
			return _socket.connected;
		}
		
		public function close():void
		{
			_socket.close();
		}
		
		public function sendCommand(data:ByteArray):int{
			if(data.length == as3kinect.COMMAND_SIZE){
				_socket.writeBytes(data, 0, as3kinect.COMMAND_SIZE);
				_socket.flush();
				return as3kinect.SUCCESS;
			} else {
				throw new Error( 'Incorrect data size (' + data.length + '). Expected: ' + as3kinect.COMMAND_SIZE);
				return as3kinect.ERROR;
			}
		}
		
		private function onSocketData(event:ProgressEvent):void
		{
			if(_socket.bytesAvailable > 0) {
				if(_packet_size == 0) {
					_socket.endian = Endian.LITTLE_ENDIAN;
					_first_byte = _socket.readByte();
					_second_byte = _socket.readByte();
					_packet_size = _socket.readInt();
				}
				if(_socket.bytesAvailable >= _packet_size && _packet_size != 0){
					_socket.readBytes(_buffer, 0, _packet_size);
					_buffer.endian = Endian.LITTLE_ENDIAN;
					_buffer.position = 0;
					_data_obj.first = _first_byte;
					_data_obj.second = _second_byte;
					_data_obj.buffer = _buffer;
					_packet_size = 0;
					dispatchEvent(new as3kinectSocketEvent(as3kinectSocketEvent.ONDATA, _data_obj));
				}
			}
		}
		
		private function onSocketError(event:IOErrorEvent):void{
			dispatchEvent(new as3kinectSocketEvent(as3kinectSocketEvent.ONERROR, null));
		}
		
		private function onSocketConnect(event:Event):void{
			dispatchEvent(new as3kinectSocketEvent(as3kinectSocketEvent.ONCONNECT, null));
		}

		public function set instance(instance:as3kinectSocket):void 
		{
			throw new Error('as3kinectSocket.instance is read-only');
		}
		
		public static function get instance():as3kinectSocket 
		{
			if ( _instance == null )
			{
				_singleton_lock = true;
				_instance = new as3kinectSocket();
				_singleton_lock = false;
			}
			return _instance;
		}
	}
}