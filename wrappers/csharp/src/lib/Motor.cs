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

namespace LibFreenect
{
	/// <summary>
	/// Provides control over the Motor on the Kinect
	/// </summary>
	/// <author>Aditya Gaddam (adityagaddam@gmail.com)</author>
	/// 
	public class Motor
	{
		/// <summary>
		/// Parent Kinect instance
		/// </summary>
		private Kinect parentDevice;
		
		/// <summary>
		/// Current 
		/// </summary>
		private float commandedTilt;
		
		/// <summary>
		/// Gets the commanded tilt value [-1.0, 1.0] for the motor. This is just the tilt
		/// value that the motor was last asked to go to through Motor.Tilt. This doesn't 
		/// correspond to the actual angle at the physical motor. For that value, see Motor.Tilt.
		/// </summary>
		public float CommandedTilt
		{
			get
			{
				return this.commandedTilt;
			}
		}
		
		/// <summary>
		/// Gets the actual raw tilt value of the motor on the kinect.
		/// </summary>
		public int RawTilt
		{
			get
			{
				return (int)this.parentDevice.cachedDeviceState.TiltAngle;
			}
		}
		
		/// <summary>
		/// Gets or sets the tilt angle of the motor on the Kinect device.
		/// Accepted values are [-1.0, 1.0]. When queried, this returns the ACTUAL
		/// tilt value/status of the motor. To get the commanded tilt value after 
		/// setting this value, you can use the Motor.CommandedTilt property.
		/// </summary>
		public float Tilt
		{
			get
			{
				return this.GetMotorTilt();
			}
			set
			{
				this.SetMotorTilt(value);
			}
		}
		
		/// <summary>
		/// Gets the status of the tilt motor.
		/// </summary>
		public TiltStatusOption TiltStatus
		{
			get
			{
				return this.parentDevice.cachedDeviceState.TiltStatus;
			}
		}
		
		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="parent">
		/// Parent <see cref="Kinect"/> device that this Motor is part of
		/// </param>
		internal Motor(Kinect parent)
		{
			this.parentDevice = parent;
			
			// Set tilt to 0 to start
			this.Tilt = 0;
		}
		
		/// <summary>
		/// Gets the motor's actual tilt angle
		/// </summary>
		/// <returns>
		/// Actual tilt angle of the motor as it's moving
		/// </returns>
		private float GetMotorTilt()
		{
			if(this.parentDevice.cachedDeviceState.TiltAngle == -128)
			{
				return -2.0f;
			}
			return (float)this.parentDevice.cachedDeviceState.TiltAngle / 61.0f;
		}
		
		/// <summary>
		/// Sets the motor's tilt angle.
		/// </summary>
		/// <param name="angle">
		/// Value between [-1.0, 1.0]
		/// </param>
		private void SetMotorTilt(float angle)
		{
			// Check if value is in valid ranges
			if(angle < -1.0 || angle > 1.0)
			{
				throw new ArgumentOutOfRangeException("Motor tilt has to be in the range [-1.0, 1.0]");
			}
			
			// Figure out raw angle between -31 and 31
			double rawAngle = Math.Round(angle * 31);
			
			// Call native func.
			int result = KinectNative.freenect_set_tilt_degs(this.parentDevice.devicePointer, rawAngle);
			if(result != 0)
			{
				throw new Exception("Coult not set raw motor tilt angle to " + angle + ". Error Code: " + result);
			}
			
			// Save commanded tilt
			this.commandedTilt = angle;
		}
		
		/// <summary>
		/// Different states the tilt motor can be in operation
		/// </summary>
		public enum TiltStatusOption
		{
			Stopped 	= 0x00,
		 	AtLimit 	= 0x01,
			Moving 		= 0x04
		}
	}
}

