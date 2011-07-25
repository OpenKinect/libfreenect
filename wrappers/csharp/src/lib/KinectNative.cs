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

namespace freenect
{
	/// <summary>
	/// Provides access to native libfreenect calls. These are "ugly" calls used internally 
	/// in the wrapper.
	/// </summary>
	///
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
		/// Callback delegate for log messages coming in from the C library.
		/// </summary>
		private static FreenectLogCallback LogCallback = new FreenectLogCallback(Kinect.LogCallback);
		
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
			KinectNative.freenect_set_log_callback(KinectNative.freenectContext, LogCallback);
		}

		[DllImport("freenect", CallingConvention=CallingConvention.Cdecl)]
		public static extern int freenect_init(ref IntPtr context, IntPtr freenectUSBContext);
		
		[DllImport("freenect", CallingConvention=CallingConvention.Cdecl)]
		public static extern int freenect_shutdown(IntPtr context);
		
		[DllImport("freenect", CallingConvention=CallingConvention.Cdecl)]
		public static extern void freenect_set_log_level(IntPtr context, LoggingLevel level);
		
		[DllImport("freenect", CallingConvention=CallingConvention.Cdecl)]
		public static extern void freenect_set_log_callback(IntPtr context, FreenectLogCallback callback);
		
		[DllImport("freenect", CallingConvention=CallingConvention.Cdecl)]
		public static extern int freenect_process_events(IntPtr context);
		
		[DllImport("freenect", CallingConvention=CallingConvention.Cdecl)]
		public static extern int freenect_num_devices(IntPtr context);
		
		[DllImport("freenect", CallingConvention=CallingConvention.Cdecl)]
		public static extern int freenect_open_device(IntPtr context, ref IntPtr device, int index);
		
		[DllImport("freenect", CallingConvention=CallingConvention.Cdecl)]
		public static extern int freenect_close_device(IntPtr device);
		
		[DllImport("freenect", CallingConvention=CallingConvention.Cdecl)]
		public static extern void freenect_set_depth_callback(IntPtr device, FreenectCameraDataCallback callback);
		
		[DllImport("freenect", CallingConvention=CallingConvention.Cdecl)]
		public static extern void freenect_set_video_callback(IntPtr device, FreenectCameraDataCallback callback);
		
		[DllImport("freenect", CallingConvention=CallingConvention.Cdecl)]
		public static extern int freenect_set_depth_buffer(IntPtr device, IntPtr buf);
		
		[DllImport("freenect", CallingConvention=CallingConvention.Cdecl)]
		public static extern int freenect_set_video_buffer(IntPtr device, IntPtr buf);
		
		[DllImport("freenect", CallingConvention=CallingConvention.Cdecl)]
		public static extern int freenect_start_depth(IntPtr device);
		
		[DllImport("freenect", CallingConvention=CallingConvention.Cdecl)]
		public static extern int freenect_start_video(IntPtr device);
		
		[DllImport("freenect", CallingConvention=CallingConvention.Cdecl)]
		public static extern int freenect_stop_depth(IntPtr device);
		
		[DllImport("freenect", CallingConvention=CallingConvention.Cdecl)]
		public static extern int freenect_stop_video(IntPtr device);
		
		[DllImport("freenect", CallingConvention=CallingConvention.Cdecl)]
		public static extern int freenect_update_tilt_state(IntPtr device);
		
		[DllImport("freenect", CallingConvention=CallingConvention.Cdecl)]
		public static extern IntPtr freenect_get_tilt_state(IntPtr device);
		
		[DllImport("freenect", CallingConvention=CallingConvention.Cdecl)]
		public static extern double freenect_get_tilt_degs(IntPtr device);
		
		[DllImport("freenect", CallingConvention=CallingConvention.Cdecl)]
		public static extern int freenect_set_tilt_degs(IntPtr device, double angle);
		
		[DllImport("freenect", CallingConvention=CallingConvention.Cdecl)]
		public static extern MotorTiltStatus freenect_get_tilt_status(IntPtr device);
		
		[DllImport("freenect", CallingConvention=CallingConvention.Cdecl)]
		public static extern int freenect_set_led(IntPtr device, LEDColor color);	
		
		[DllImport("freenect", CallingConvention=CallingConvention.Cdecl)]
		public static extern int freenect_get_video_mode_count(IntPtr device);
		
		[DllImport("freenect", CallingConvention=CallingConvention.Cdecl)]
		public static extern FreenectFrameMode freenect_get_video_mode(int modeNum);
		
		[DllImport("freenect", CallingConvention=CallingConvention.Cdecl)]
		public static extern FreenectFrameMode freenect_get_current_video_mode();
		
		[DllImport("freenect", CallingConvention=CallingConvention.Cdecl)]
		public static extern FreenectFrameMode freenect_find_video_mode(Resolution resolution, VideoFormat videoFormat);
		
		[DllImport("freenect", CallingConvention=CallingConvention.Cdecl)]
		public static extern int freenect_set_video_mode(IntPtr device, FreenectFrameMode mode);
		
		[DllImport("freenect", CallingConvention=CallingConvention.Cdecl)]
		public static extern int freenect_get_depth_mode_count(IntPtr device);
		
		[DllImport("freenect", CallingConvention=CallingConvention.Cdecl)]
		public static extern FreenectFrameMode freenect_get_depth_mode(int modeNum);
		
		[DllImport("freenect", CallingConvention=CallingConvention.Cdecl)]
		public static extern FreenectFrameMode freenect_get_current_depth_mode();
		
		[DllImport("freenect", CallingConvention=CallingConvention.Cdecl)]
		public static extern FreenectFrameMode freenect_find_depth_mode(Resolution resolution, DepthFormat depthFormat);
		
		[DllImport("freenect", CallingConvention=CallingConvention.Cdecl)]
		public static extern int freenect_set_depth_mode(IntPtr device, FreenectFrameMode mode);
		
	}
	
	/// <summary>
	/// Frame capture settings for all video/depth feeds
	/// </summary>
	[StructLayout(LayoutKind.Explicit)]
	internal struct FreenectFrameMode
	{
		[FieldOffset(0)]
		public UInt32 Reserved;
		[FieldOffset(4)]
		public Resolution Resolution;
		[FieldOffset(8)]
		public VideoFormat VideoFormat;
		[FieldOffset(8)]
		public DepthFormat DepthFormat;
		[FieldOffset(12)]
		public int Bytes;
		[FieldOffset(16)]
		public short Width;
		[FieldOffset(18)]
		public short Height;
		[FieldOffset(20)]
		public byte DataBitsPerPixel;
		[FieldOffset(21)]
		public byte PaddingBitsPerPixel;
		[FieldOffset(22)]
		public byte Framerate;
		[FieldOffset(23)]
		public byte IsValid;
	}
	
	/// <summary>
	/// Device tilt state values. This holds stuff like accel and tilt status
	/// </summary>
	internal struct FreenectTiltState
	{
		public Int16 			AccelerometerX;
		public Int16 			AccelerometerY;
		public Int16 			AccelerometerZ;
		public SByte  			TiltAngle;
		public MotorTiltStatus  TiltStatus;
	}
	
	/// <summary>
	/// "Native" callback for freelect library logging
	/// </summary>
	[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
	public delegate void FreenectLogCallback(IntPtr device, LoggingLevel logLevel, string message);
	
	/// <summary>
	/// "Native" callback for camera data
	/// </summary>
	[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
	public delegate void FreenectCameraDataCallback(IntPtr device, IntPtr data, UInt32 timestamp);
}
