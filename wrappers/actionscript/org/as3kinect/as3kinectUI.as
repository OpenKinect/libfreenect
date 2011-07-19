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

package org.as3kinect
{
	import com.bit101.components.*;
	import com.bit101.charts.*;
	
	import flash.display.DisplayObjectContainer;
	import flash.events.MouseEvent;
	
	import org.as3kinect.as3kinectWrapper;
	import org.as3kinect.objects.motorData;	
	import flash.events.Event;
	
	public class as3kinectUI
	{
		var _as3w	:as3kinectWrapper;
		var info	:Text;
		public var depth_enabled	:Boolean = true;
		public var video_enabled	:Boolean = true;
		public var motor_enabled	:Boolean = true;
		public var blobs_on			:Boolean = false;
		public var wb_filter_on		:Boolean = false;
		public var threshold_value	:int = 50;
		
		public function as3kinectUI(wrapper:as3kinectWrapper):void{
			_as3w = wrapper;
		}
		
		public function showLedPanel(where:DisplayObjectContainer, _x:int, _y:int, _closed:Boolean = false, _draggable:Boolean = true, _color:int = 0xFFFFFF):void{
			var ledControl:Window = new Window(where, _x, _y, "Led Control");
			ledControl.minimized = _closed;
			ledControl.draggable = _draggable;
			ledControl.color = _color;
			ledControl.hasMinimizeButton = true;
			ledControl.width = 110;
			ledControl.height = 175;

			new PushButton(ledControl.content, 5, 5, "Off", function(e:MouseEvent){
				_as3w.motor.ledColor = 0;
			});

			new PushButton(ledControl.content, 5, 30, "Green", function(e:MouseEvent){
				_as3w.motor.ledColor = 1;
			});

			new PushButton(ledControl.content, 5, 55, "Red", function(e:MouseEvent){
				_as3w.motor.ledColor = 2;
			});

			new PushButton(ledControl.content, 5, 80, "Orange", function(e:MouseEvent){
				_as3w.motor.ledColor = 3;
			});

			new PushButton(ledControl.content, 5, 105, "Blink (Green)", function(e:MouseEvent){
				_as3w.motor.ledColor = 4;
			});

			new PushButton(ledControl.content, 5, 130, "Blink (Red)",function(e:MouseEvent){
				_as3w.motor.ledColor = 6;
			});
		}
		
		public function showDepthPanel(where:DisplayObjectContainer, _x:int, _y:int, _closed:Boolean = false, _draggable:Boolean = true, _color:int = 0xFFFFFF):void {
			var depthControl:Window = new Window(where, _x, _y, "Depth Control");
			depthControl.minimized = _closed;
			depthControl.draggable = _draggable;
			depthControl.color = _color;
			depthControl.hasMinimizeButton = true;
			depthControl.width = 185;
			depthControl.height = 150;
			
			//Enable depth transmission			
			var d_enabled:CheckBox = new CheckBox(depthControl.content, 5, 5, "Enabled", function(e:MouseEvent){
				depth_enabled = e.target.selected;
			});
			d_enabled.selected = depth_enabled;
			
			//Switch depth mirroring			
			new CheckBox(depthControl.content, 5, 20, "Mirror image", function(e:MouseEvent){
				_as3w.depth.mirrored = e.target.selected;
			});
			
			//Switch blob generation
			var blob_process:CheckBox = new CheckBox(depthControl.content, 5, 35, "Process blobs", function(e:MouseEvent){
				blobs_on = e.target.selected;
				//Blob generation requires white/black filtering on
				if(blobs_on) bw_filter.selected = wb_filter_on = true;
			});

			//Switch black/white filter
			var bw_filter:CheckBox = new CheckBox(depthControl.content, 5, 50, "Enable BW Filter",function(e:MouseEvent){
				wb_filter_on = e.target.selected;
				//Blob generation requires white/black filtering on
				if(!wb_filter_on) {
					blob_process.selected = blobs_on = false;
				}
			});
			
			new Label(depthControl.content, 5, 70, "Range");
			
			//Depth range slider
			var d_range:HRangeSlider = new HRangeSlider(depthControl.content, 40, 80, function(e:Event){
				_as3w.depth.maxDistance = e.target.highValue;
				_as3w.depth.minDistance = e.target.lowValue;
			});
			d_range.maximum = 2000;	
			d_range.width = 125;
			d_range.highValue = _as3w.depth.maxDistance;
			d_range.lowValue = _as3w.depth.minDistance;
			
			//JPEG quality slider
			var d_quality:HUISlider = new HUISlider(depthControl.content, 5, 95, "Quality", function(e:Event){
				_as3w.depth.compression = int(e.target.value);
			});
			d_quality.value = _as3w.depth.compression;
			
			//Threshold level slider
			var th_value:HUISlider = new HUISlider(depthControl.content, 5, 110, "Threshold", function(e:Event){
				threshold_value = e.target.value;
			});
			th_value.maximum = 255;
			th_value.value = threshold_value;
		}
		
		public function showVideoPanel(where:DisplayObjectContainer, _x:int, _y:int, _closed:Boolean = false, _draggable:Boolean = true, _color:int = 0xFFFFFF):void {
			var videoControl:Window = new Window(where, _x, _y, "Video Control");
			videoControl.minimized = _closed;
			videoControl.draggable = _draggable;
			videoControl.color = _color;
			videoControl.hasMinimizeButton = true;
			videoControl.width = 185;
			videoControl.height = 75;
			
			//Enable video transmission			
			var v_enabled:CheckBox = new CheckBox(videoControl.content, 5, 5, "Enabled", function(e:MouseEvent){
				video_enabled = e.target.selected;
			});
			v_enabled.selected = video_enabled;
			
			//Switch video mirroring			
			new CheckBox(videoControl.content, 5, 20, "Mirror image", function(e:MouseEvent){
				_as3w.video.mirrored = e.target.selected;
			});
			
			//JPEG quality slider
			var v_quality:HUISlider = new HUISlider(videoControl.content, 5, 35, "Quality", function(e:Event){
				_as3w.video.compression = int(e.target.value);
			});
			v_quality.value = _as3w.video.compression;
		}
		
		public function showMotorPanel(where:DisplayObjectContainer, _x:int, _y:int, _closed:Boolean = false, _draggable:Boolean = true, _color:int = 0xFFFFFF):void {
			var motorControl:Window = new Window(where, _x, _y, "Motor Control");
			motorControl.minimized = _closed;
			motorControl.draggable = _draggable;
			motorControl.color = _color;
			motorControl.hasMinimizeButton = true;
			motorControl.width = 185;
			motorControl.height = 65;
			
			//Enable motor transmission			
			var m_enabled:CheckBox = new CheckBox(motorControl.content, 5, 5, "Enabled", function(e:MouseEvent){
				motor_enabled = e.target.selected;
			});
			m_enabled.selected = motor_enabled;
			
			var m_position:HUISlider = new HUISlider(motorControl.content, 5, 20, "Angle", function(e:Event){
				_as3w.motor.position = int(e.target.value);
			});
			m_position.tick = 1;
			m_position.minimum = -31;
			m_position.maximum = 31;
			m_position.value = _as3w.motor.position;
		}
		
		public function showAccelerometersDisplay(where:DisplayObjectContainer, _x:int, _y:int, _closed:Boolean = false, _draggable:Boolean = true, _color:int = 0xFFFFFF):void {
			var motorData:Window = new Window(where, _x, _y, "Motor Data");
			motorData.minimized = _closed;
			motorData.draggable = _draggable;
			motorData.color = _color;
			motorData.hasMinimizeButton = true;
			motorData.width = 185;
			motorData.height = 150;
			info = new Text(motorData.content);
			info.text = "raw acceleration:\n\tax: 0\n\tay: 0\n\taz: 0\n\n";
			info.text += "mks acceleration:\n\tdx: 0\n\tdy: 0\n\tdz: 0";
			info.height = 150;
		}
		
		public function updateAccelerometerDisplay(object:motorData):void {
			info.text = "raw acceleration:\n\tax: " + object.ax + "\n\tay: " + object.ay + "\n\taz: " + object.az + "\n\n";
			info.text += "mks acceleration:\n\tdx: " + object.dx + "\n\tdy: " + object.dy + "\n\tdz: " + object.dz + "\n";
		}
	}
}