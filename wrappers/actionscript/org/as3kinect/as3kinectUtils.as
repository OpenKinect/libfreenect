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
	import org.as3kinect.as3kinect;
	
	import flash.display.BitmapData;
	import flash.display.Stage;
	import flash.display.DisplayObject;
	
	import flash.geom.Rectangle;
	import flash.geom.Point;
	
	import flash.filters.ColorMatrixFilter;
	import flash.events.TouchEvent;
	import flash.utils.ByteArray;
	
	public class as3kinectUtils
	{
		
		/*
		 * Draw ARGB from ByteArray to BitmapData object
		 */
		public static function byteArrayToBitmapData(bytes:ByteArray, _canvas:BitmapData):void{
			_canvas.lock();
			_canvas.setPixels(new Rectangle(0,0, as3kinect.IMG_WIDTH, as3kinect.IMG_HEIGHT), bytes);
			_canvas.unlock();
		}
		
		/*
		 * Process blobs from BitmapData, if _w and _h set they will be returned in that resoluton
		 */		
		public static function getBlobs(r:BitmapData, _w:Number = 0, _h:Number = 0):Array 
		{
			//if _w and _h not specified use as3kinect constants
			if(_w == 0) _w = as3kinect.IMG_WIDTH;
			if(_h == 0) _h = as3kinect.IMG_HEIGHT;
			
			var i:int;
			var blobs:Array = new Array();
			
			//Looking fot the blobs
			while (i < as3kinect.MAX_BLOBS)
			{
			    //Look for BLOB_COLOR in the BitmapData and genrate a rectanglo enclosing the Blobs of that color
				var mainRect:Rectangle = r.getColorBoundsRect(as3kinect.BLOB_MASK, as3kinect.BLOB_COLOR);
				//No blobs found (Exit)
			    if (mainRect.isEmpty()) break;
			    var xx:int = mainRect.x;
				//Looking in mainRect for a pixel with BLOB_COLOR
			    for (var yy:uint = mainRect.y; yy < mainRect.y + mainRect.height; yy++)
			    {
			        if (r.getPixel32(xx, yy) == as3kinect.BLOB_COLOR)
			        {
						//Use floodFill to paint that blob to BLOB_FILL_COLOR (works like paint fill tool)
			            r.floodFill(xx, yy, as3kinect.BLOB_FILL_COLOR);
						//Now our blob is not BLOB_COLOR we get the rect of our recently painted blob (this is our i blob)
			            var blobRect:Rectangle = r.getColorBoundsRect(as3kinect.BLOB_MASK, as3kinect.BLOB_FILL_COLOR);
						//Looking if our blob fits our desired min/max width and height
			            if (blobRect.width > as3kinect.BLOB_MIN_WIDTH 
							&& blobRect.width < as3kinect.BLOB_MAX_WIDTH 
							&& blobRect.height > as3kinect.BLOB_MIN_HEIGHT 
							&& blobRect.height < as3kinect.BLOB_MAX_HEIGHT)
			            {
							//Create the blob object
			                var blob:Object = {};
							//Add a rect to the object
			                blob.rect = blobRect;
							//Convert blob positions to float and then multiply per requested width and height
							blob.rect.x = ((blob.rect.x / as3kinect.IMG_WIDTH) * _w);
							blob.rect.y = ((blob.rect.y / as3kinect.IMG_HEIGHT) * _h);
							blob.rect.width = (blob.rect.width / as3kinect.IMG_WIDTH) * _w;
							blob.rect.height = (blob.rect.height / as3kinect.IMG_HEIGHT) * _h;
							//The point is the center of the rect
							var _x:int = blob.rect.x + (blob.rect.width / 2);
							var _y:int = blob.rect.y + (blob.rect.height / 2);
							//Add a point to the object
							blob.point = new Point(_x, _y);
			                blobs.push(blob);
			            }
						//Finally we paint our blob to a BLOB_PROCESSED_COLOR so we can discard it in the next pass
			            r.floodFill(xx, yy, as3kinect.BLOB_PROCESSED_COLOR);
			        }
			    }
			    i++;
			}
			return blobs;
		}
		
		/*
		 * Convert a BitmapData into black and white BitmapData with a ColorMatrixFilter
		 */		
		public static function setBlackWhiteFilter(obj:BitmapData, threshold:int = 128):void
		{
			var rLum:Number = 0.2225;
			var gLum:Number = 0.7169;
			var bLum:Number = 0.0606;
			var matrix:Array = [rLum * 256, gLum * 256, bLum * 256, 0, -256 * threshold,
								rLum * 256, gLum * 256, bLum * 256, 0, -256 * threshold,
								rLum * 256, gLum * 256, bLum * 256, 0, -256 * threshold,
								0,          0,          0,          1, 0                ]; 
			var filter:ColorMatrixFilter = new ColorMatrixFilter(matrix);
			obj.applyFilter(obj, new Rectangle(0,0,obj.width,obj.height), new Point(0,0), filter);
		}
		
		/*
		 * Fire a touch event in the desired point
		 * Id is used so we can detect the TouchOut of that specific point
		 * _lastTouched Array is used to store whether this point has already been fired or not
		 * _stage is needed to have access to getObjectsUnderPoint
		 */		
		public static function fireTouchEvent(id:int, point:Point,_lastTouched:Array, _stage:Stage):void
		{
			if(_lastTouched[id] == undefined) _lastTouched[id] = new Array;
			var targets:Array = _stage.getObjectsUnderPoint(point);
			var i:int;
			var max:int;
			var found:Boolean;
			var local:Point;
			//We look in the lastTouched arrar for this point id
			for (var key in _lastTouched[id])
			{
				//If TOUCH_OVER was already fired we look for it in the new targets object
				if (_lastTouched[id][key].bool != false)
				{
					found = false;
					for (i = 0,max = targets.length; i < max; i++)
					{
						if (targets[i].name == _lastTouched[id][key].obj.name)
						{
							found = true;
							break;
						}
					}
					//If is not in the targets object we fire the TOUCH_OUT event
					if (found == false)
					{
						_lastTouched[id][key] = {bool:false,obj:_lastTouched[id][key].obj};
						local = _lastTouched[id][key].obj.parent.globalToLocal(new Point(point.x,point.y));
						_lastTouched[id][key].obj.dispatchEvent(new TouchEvent(TouchEvent.TOUCH_OUT,true,false,0,false,local.x,local.y));
					}
				}
			}
			//We look now into the targets object
			for (i = 0,max = targets.length; i < max; i++)
			{
				var item:DisplayObject = targets[i];
				local = item.parent.globalToLocal(new Point(point.x,point.y));
				//if the object is new (was not in the lastTouched array OR the TOUCH_OVER is not already fired we fire TOUCH_OVER event
				if (! _lastTouched[id][item.name] || _lastTouched[id][item.name].bool !== true)
				{
					_lastTouched[id][item.name] = {bool:true,obj:item};
					item.dispatchEvent(new TouchEvent(TouchEvent.TOUCH_OVER,true,false,0,false,local.x,local.y));
				}
			}
		}
	}
}