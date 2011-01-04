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
	import flash.geom.Rectangle;
	import flash.geom.Point;
	
	public class as3kinectBlobs
	{
		public static function getBlobs(r:BitmapData, _w:Number = 0, _h:Number = 0):Array 
		{
			var i:int;
			var blobs:Array = new Array();
			while (i < as3kinect.MAX_BLOBS)
			{
			    var mainRect:Rectangle = r.getColorBoundsRect(as3kinect.BLOB_MASK, as3kinect.BLOB_COLOR);
			    if (mainRect.isEmpty()) break;
			    var xx:int = mainRect.x;
			    for (var yy:uint = mainRect.y; yy < mainRect.y + mainRect.height; yy++)
			    {
			        if (r.getPixel32(xx, yy) == as3kinect.BLOB_COLOR)
			        {
			            r.floodFill(xx, yy, as3kinect.BLOB_FILL_COLOR);
			            var blobRect:Rectangle = r.getColorBoundsRect(as3kinect.BLOB_MASK, as3kinect.BLOB_FILL_COLOR);
			            if (blobRect.width > as3kinect.BLOB_MIN_WIDTH 
							&& blobRect.width < as3kinect.BLOB_MAX_WIDTH 
							&& blobRect.height > as3kinect.BLOB_MIN_HEIGHT 
							&& blobRect.height < as3kinect.BLOB_MAX_HEIGHT)
			            {
			                var blob:Object = {};
			                blob.rect = blobRect;
							var _x:int = blob.rect.x + (blob.rect.width / 2);
							var _y:int = blob.rect.y + (blob.rect.height / 2);
							if(_w != 0 && _h != 0) {
								blob.rect.x = blob.rect.x / as3kinect.IMG_WIDTH * _w;
								blob.rect.y = blob.rect.y / as3kinect.IMG_HEIGHT * _h;
								blob.rect.width = blob.rect.width / as3kinect.IMG_WIDTH * _w;
								blob.rect.height = blob.rect.height / as3kinect.IMG_HEIGHT * _h;
								_x = _x / as3kinect.IMG_WIDTH * _w;
								_y = _y / as3kinect.IMG_HEIGHT * _h;
							}						
							blob.point = new Point(_x, _y);
			                blobs.push(blob);
			            }
			            r.floodFill(xx, yy, as3kinect.BLOB_PROCESSED_COLOR);
			        }
			    }
			    i++;
			}
			return blobs;
		}
	}
}