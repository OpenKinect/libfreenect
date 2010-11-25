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
using System.Threading;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace LibFreenect
{
	/// <summary>
	/// Provides access to native libfreenect calls. These are "ugly" calls used internally 
	/// in the wrapper.
	/// </summary>
	/// <author>Aditya Gaddam (adityagaddam@gmail.com)</author>
	/// 
	class KinectNative
	{
		/// <summary>
		/// Main freenect context. There is one per session.
		/// </summary>
		private static IntPtr freenectContext = IntPtr.Zero;
		
		/// <summary>
		/// Map between native pointers to actual Kinect devices
		/// </summary>
		private static Dictionary<IntPtr, Kinect> deviceMap = new Dictionary<IntPtr, Kinect>();
		
		/// <summary>
		/// Gets a freenect context to work with.
		/// </summary>
		public static IntPtr Context
		{
			get
			{
				// Make sure we have a context
				if(KinectNative.freenectContext == IntPtr.Zero)
				{
					KinectNative.InitializeContext();
				}
				
				// Return it
				return KinectNative.freenectContext;
			}
		}
		
		/// <summary>
		/// Shuts down the context and closes any open devices.
		/// </summary>
		public static void ShutdownContext()
		{
			// Close all devices
			foreach(Kinect device in KinectNative.deviceMap.Values)
			{
				device.Close();
			}
			
			// Shutdown context
			int result = KinectNative.freenect_shutdown(KinectNative.freenectContext);
			if(result != 0)
			{
				throw new Exception("Could not shutdown freenect context. Error Code:" + result);
			}
			
			// Dispose pointer
			KinectNative.freenectContext = IntPtr.Zero;
		}
		
		/// <summary>
		/// Gets a kinect device given it's native pointer. This is 
		/// useful for callbacks.
		/// </summary>
		/// <param name="pointer">
		/// A <see cref="IntPtr"/>
		/// </param>
		/// <returns>
		/// A <see cref="Kinect"/>
		/// </returns>
		public static Kinect GetDevice(IntPtr pointer)
		{
			if(KinectNative.deviceMap.ContainsKey(pointer) == false)
			{
				return null;
			}
			return KinectNative.deviceMap[pointer];
		}
		
		/// <summary>
		/// Registers a device and its native pointer
		/// </summary>
		/// <param name="pointer">
		/// A <see cref="IntPtr"/>
		/// </param>
		/// <param name="device">
		/// A <see cref="Kinect"/>
		/// </param>
		public static void RegisterDevice(IntPtr pointer, Kinect device)
		{
			if(KinectNative.deviceMap.ContainsKey(pointer))
			{
				KinectNative.deviceMap.Remove(pointer);
			}
			KinectNative.deviceMap.Add(pointer, device);
		}
		
		/// <summary>
		/// Unregister the device pointed to by the specified native pointer.
		/// </summary>
		/// <param name="pointer">
		/// A <see cref="IntPtr"/>
		/// </param>
		public static void UnregisterDevice(IntPtr pointer)
		{
			if(KinectNative.deviceMap.ContainsKey(pointer))
			{
				KinectNative.deviceMap.Remove(pointer);
			}
		}
		
		/// <summary>
		/// Initializes the freenect context
		/// </summary>
		private static void InitializeContext()
		{
			int result = KinectNative.freenect_init(ref KinectNative.freenectContext, IntPtr.Zero);
			if(result != 0)
			{
				throw new Exception("Could not initialize freenect context. Error Code:" + result);
			}
			
			// Set callbacks for logging
			KinectNative.freenect_set_log_callback(KinectNative.freenectContext, new FreenectLogCallback(Kinect.LogCallback));
		}

		[DllImport("libfreenect")]
		public static extern int freenect_init(ref IntPtr context, IntPtr freenectUSBContext);
		
		[DllImport("libfreenect")]
		public static extern int freenect_process_events(IntPtr context);
		
		[DllImport("libfreenect")]
		public static extern void freenect_set_log_level(IntPtr context, Kinect.LogLevelOptions level);
		
		[DllImport("libfreenect")]
		public static extern void freenect_set_log_callback(IntPtr context, FreenectLogCallback callback);
		
		[DllImport("libfreenect")]
		public static extern int freenect_shutdown(IntPtr context);
		
		[DllImport("libfreenect")]
		public static extern int freenect_num_devices(IntPtr context);
		
		[DllImport("libfreenect")]
		public static extern int freenect_open_device(IntPtr context, ref IntPtr device, int index);
		
		[DllImport("libfreenect")]
		public static extern int freenect_close_device(IntPtr device);
		
		[DllImport("libfreenect")]
		public static extern int freenect_set_led(IntPtr device, LED.ColorOption option);
		
		[DllImport("libfreenect")]
		public static extern int freenect_set_tilt_degs(IntPtr device, double angle);
		
		[DllImport("libfreenect")]
		public static extern int freenect_set_video_format(IntPtr device, VideoCamera.DataFormatOption rgbFormat);
		
		[DllImport("libfreenect")]
		public static extern void freenect_set_video_callback(IntPtr device, FreenectVideoDataCallback callback);
		
		[DllImport("libfreenect")]
		public static extern int freenect_start_video(IntPtr device);
		
		[DllImport("libfreenect")]
		public static extern int freenect_stop_video(IntPtr device);
		
		[DllImport("libfreenect")]
		public static extern int freenect_set_depth_format(IntPtr device, DepthCamera.DataFormatOption depthFormat);
		
		[DllImport("libfreenect")]
		public static extern void freenect_set_depth_callback(IntPtr device, FreenectDepthDataCallback callback);
		
		[DllImport("libfreenect")]
		public static extern int freenect_start_depth(IntPtr device);
		
		[DllImport("libfreenect")]
		public static extern int freenect_stop_depth(IntPtr device);
		
		[DllImport("libfreenect")]
		public static extern int freenect_update_tilt_state(IntPtr device);
		
		[DllImport("libfreenect")]
		public static extern IntPtr freenect_get_tilt_state(IntPtr device);
		
		[DllImport("libfreenect")]
		public static extern int freenect_set_depth_buffer(IntPtr device, IntPtr buf);
		
		[DllImport("libfreenect")]
		public static extern int freenect_set_video_buffer(IntPtr device, IntPtr buf);
	}
	
	/// <summary>
	/// Device tilt state values. This holds stuff like accel and tilt status
	/// </summary>
	internal struct FreenectTiltState
	{
		public Int16 					AccelerometerX;
		public Int16 					AccelerometerY;
		public Int16 					AccelerometerZ;
		public SByte  					TiltAngle;
		public Motor.TiltStatusOption  TiltStatus;
	}
	
	/// <summary>
	/// "Native" callback for freelect library logging
	/// </summary>
	delegate void FreenectLogCallback(IntPtr device, Kinect.LogLevelOptions logLevel, string message);
	
	/// <summary>
	/// "Native" callback for depth data
	/// </summary>
	delegate void FreenectDepthDataCallback(IntPtr device, IntPtr depthData, UInt32 timestamp);
	
	/// <summary>
	/// "Native" callback for video image data
	/// </summary>
	delegate void FreenectVideoDataCallback(IntPtr device, IntPtr imageData, UInt32 timestamp);
	
}
