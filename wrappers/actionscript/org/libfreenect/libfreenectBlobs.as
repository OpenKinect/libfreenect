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

package org.libfreenect
{
	import org.libfreenect.libfreenect;
	
	import flash.display.BitmapData;
	import flash.geom.Rectangle;
	import flash.geom.Point;
	
	public class libfreenectBlobs
	{
		public static function getBlobs(r:BitmapData):Array 
		{
			var i:int;
			var blobs:Array = new Array();
			while (i < libfreenect.MAX_BLOBS)
			{
			    var mainRect:Rectangle = r.getColorBoundsRect(libfreenect.BLOB_MASK, libfreenect.BLOB_COLOR);
			    if (mainRect.isEmpty()) break;
			    var xx:int = mainRect.x;
			    for (var yy:uint = mainRect.y; yy < mainRect.y + mainRect.height; yy++)
			    {
			        if (r.getPixel32(xx, yy) == libfreenect.BLOB_COLOR)
			        {
			            r.floodFill(xx, yy, libfreenect.BLOB_FILL_COLOR);
			            var blobRect:Rectangle = r.getColorBoundsRect(libfreenect.BLOB_MASK, libfreenect.BLOB_FILL_COLOR);
			            if (blobRect.width > libfreenect.BLOB_MIN_WIDTH 
							&& blobRect.width < libfreenect.BLOB_MAX_WIDTH 
							&& blobRect.height > libfreenect.BLOB_MIN_HEIGHT 
							&& blobRect.height < libfreenect.BLOB_MAX_HEIGHT)
			            {
			                var blob:Object = {};
			                blob.rect = blobRect;
							blob.rect.x = libfreenect.IMG_WIDTH - blob.rect.x - blob.rect.width;
							var _x:int = blob.rect.x + (blob.rect.width / 2);
							var _y:int = blob.rect.y + (blob.rect.height / 2);							
							blob.point = new Point(_x, _y);
			                blobs.push(blob);
			            }
			            r.floodFill(xx, yy, libfreenect.BLOB_PROCESSED_COLOR);
			        }
			    }
			    i++;
			}
			return blobs;
		}
	}
}