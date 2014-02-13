/*
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2010 individual OpenKinect contributors. See the CONTRIB file
 * for details.
 *
 * This code is licensed to you under the terms of the Apache License, version
 * 2.0, or, at your option, the terms of the GNU General Public License,
 * version 2.0. See the APACHE20 and GPL2 files for the text of the licenses,
 * or the following URLs:
 * http://www.apache.org/licenses/LICENSE-2.0
 * http://www.gnu.org/licenses/gpl-2.0.txt
 *
 * If you redistribute this file in source form, modified or unmodified, you
 * may:
 *   1) Leave this header intact and distribute it under the same terms,
 *      accompanying it with the APACHE20 and GPL20 files, or
 *   2) Delete the Apache 2.0 clause and accompany it with the GPL2 file, or
 *   3) Delete the GPL v2 clause and accompany it with the APACHE20 file
 * In all cases you must keep the copyright notice intact and include a copy
 * of the CONTRIB file.
 *
 * Binary distributions must follow the binary distribution requirements of
 * either License.
 */

using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Collections.Generic;

namespace freenect
{
	/// <summary>
	/// Data formats for the video camera
	/// </summary>
	public enum VideoFormat : int
	{
		RGB                 = 0,
		Bayer               = 1,
		Infrared8Bit        = 2,
		Infrared10Bit       = 3,
		InfraredPacked10Bit = 4,
		YUVRGB              = 5,
		YUVRaw              = 6
	}
	
	/// <summary>
	/// Data formats for the depth camera
	/// </summary>
	public enum DepthFormat : int
	{
		Depth11Bit 			 = 0,
		Depth10Bit 			 = 1,
		DepthPacked11Bit = 2,
		DepthPacked10Bit = 3,
		DepthRegistered  = 4,
		DepthMM          = 5
	}
	
	/// <summary>
	/// Resolution settings.
	/// 
	/// LOW = QVGA (320x240)
	/// MEDIUM = VGA (640x480 for video, 640x488 for IR)
	/// HIGH = SXGA (1280x1024)
	/// </summary>
	public enum Resolution : int
	{
		Low    = 0,
		Medium = 1,
		High   = 2
	}
	
	/// <summary>
	/// LED colors. None means LED is off.
	/// </summary>
	public enum LEDColor : int
	{
		None    		   = 0,
		Green  			   = 1,
		Red    			   = 2,
		Yellow 	       = 3,
		BlinkYellow    = 4,
		BlinkGreen     = 5,
		BlinkRedYellow = 6
	}
	
	/// <summary>
	/// Different states the tilt motor can be in operation
	/// </summary>
	public enum MotorTiltStatus : int
	{
		Stopped = 0x00,
	 	AtLimit = 0x01,
		Moving  = 0x04
	}
	
	/// <summary>
	/// Logging levels from the C library
	/// </summary>
	public enum LoggingLevel : int
	{
		Fatal = 0,
		Error,
		Warning,
		Notice,
		Info,
		Debug,
		Spew,
		Flood,
	}
}
