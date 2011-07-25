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

namespace freenect
{
	/// <summary>
	/// Provides control over the LED on the Kinect
	/// </summary>
	///
	/// 
	public class LED
	{
		/// <summary>
		/// Parent Kinect instance
		/// </summary>
		private Kinect parentDevice;
		
		/// <summary>
		/// Current color set on the LED
		/// </summary>
		private LEDColor color;
		
		/// <summary>
		/// Gets or sets the LED color on the Kinect device
		/// </summary>
		/// <value>Gets or sets 'color' field</value>
		public LEDColor Color
		{
			get
			{
				return this.color;
			}
			set
			{
				this.SetLEDColor(value);
			}
		}
		
		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="parent">
		/// Parent <see cref="Kinect"/> device that this LED is part of
		/// </param>
		internal LED(Kinect parent)
		{
			this.parentDevice = parent;
		}
		
		/// <summary>
		/// Sets the color for the LED on the Kinect.
		/// </summary>
		/// <param name="color">
		/// Color value 
		/// </param>
		private void SetLEDColor(LEDColor color)
		{
			int result = KinectNative.freenect_set_led(this.parentDevice.devicePointer, color);
			if(result != 0)
			{
				throw new Exception("Could not set color to " + color + ". Error Code:" + result);
			}
			this.color = color;
		}
	}
}

