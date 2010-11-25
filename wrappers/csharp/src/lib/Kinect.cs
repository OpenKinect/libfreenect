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
using System.Runtime.InteropServices;

namespace LibFreenect
{
	/// <summary>
	/// Kinect device. This wraps functionality associated with the entire Kinect
	/// device into a happy little bundle.
	/// </summary>
	/// <author>Aditya Gaddam (adityagaddam@gmail.com)</author>
	/// 
	public class Kinect
	{
		/// <summary>
		/// Current logging level for the kinect session (for all devices)
		/// </summary>
		private static LogLevelOptions logLevel;
		
		/// <summary>
		/// Pointer to native device object
		/// </summary>
		internal IntPtr devicePointer = IntPtr.Zero;
		
		/// <summary>
		/// Cached device state that can be used after a call to Kinect.UpdateStatus
		/// This can be used to save some USB or P/Invoke calls.
		/// </summary>
		internal FreenectTiltState cachedDeviceState;
		
		/// <summary>
		/// Gets or sets the logging level for the Kinect library. This controls
		/// how much debugging information is sent to the logging callback
		/// </summary>
		public static LogLevelOptions LogLevel
		{
			get
			{
				return Kinect.logLevel;
			}
			set
			{
				Kinect.SetLogLevel(value);
			}
		}
		
		/// <summary>
		/// Raised when a log item is received from the low level Kinect library.
		/// </summary>
		public static event LogEventHandler Log = delegate { };
		
		/// <summary>
		/// Gets the device ID for this Kinect device
		/// </summary>
		public int DeviceID
		{
			get;
			private set;
		}
		
		/// <summary>
		/// Gets whether the connection to the device is open
		/// </summary>
		public bool IsOpen
		{
			get;
			private set;
		}
		
		/// <summary>
		/// Gets the LED on this Kinect device
		/// </summary>
		public LED LED
		{
			get;
			private set;
		}
		
		/// <summary>
		/// Gets the Motor instance for this Kinect device
		/// </summary>
		public Motor Motor
		{
			get;
			private set;
		}
		
		/// <summary>
		/// Gets the accelerometer for this Kinect device
		/// </summary>
		public Accelerometer Accelerometer
		{
			get;
			private set;
		}
		
		/// <summary>
		/// Gets the RGB camera for this Kinect device
		/// </summary>
		public VideoCamera VideoCamera
		{
			get;
			private set;
		}
		
		/// <summary>
		/// Gets the depth camera for this Kinect device
		/// </summary>
		public DepthCamera DepthCamera
		{
			get;
			private set;
		}
		
		/// <summary>
		/// Gets or sets the name for this Kinect Device. 
		/// </summary>
		/// <remarks>
		/// This means nothing at all to the actual library, but can be useful for 
		/// debugging/presentation reasons. The default value is "Device {Kinect.DeviceID}" 
		/// without the curly braces. For example, "Device 0" or "Device 1". 
		/// But you can make it whatever the hell you want.
		/// </remarks>
		public string Name
		{
			get;
			set;
		}
		
		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="id">
		/// ID of the Kinect Device. This is a value in the range [0, Kinect.DeviceCount - 1]
		/// </param>
		public Kinect(int id)
		{
			// Make sure id is under  DeviceCount
			if(id >= Kinect.DeviceCount)
			{
				throw new ArgumentOutOfRangeException("The device ID has to be in the range [0, Kinect.DeviceCount - 1]");
			}
			
			// Store device ID for later
			this.DeviceID = id;
		}
		
		/// <value>
		/// Gets number of Kinect devices connected
		/// </value>
		public static int DeviceCount
		{
			get
			{
				return Kinect.GetDeviceCount();
			}
		}
		
		/// <summary>
		/// Opens up the connection to this Kinect device
		/// </summary>
		public void Open()
		{
			int result = KinectNative.freenect_open_device(KinectNative.Context, ref this.devicePointer, this.DeviceID);
			if(result != 0)
			{
				throw new Exception("Could not open connection to Kinect Device (ID=" + this.DeviceID + "). Error Code = " + result);
			}
			
			// Create child instances
			this.LED = new LED(this);
			this.Motor = new Motor(this);
			this.Accelerometer = new Accelerometer(this);
			this.VideoCamera = new VideoCamera(this);
			this.DepthCamera = new DepthCamera(this);
			
			//Register the device
			KinectNative.RegisterDevice(this.devicePointer, this);
			
			// Open now
			this.IsOpen = true;
		}
		
		/// <summary>
		/// Closes the connection to this Kinect device
		/// </summary>
		public void Close()
		{
			int result = KinectNative.freenect_close_device(this.devicePointer);
			if(result != 0)
			{
				throw new Exception("Could not close connection to Kinect Device (ID=" + this.DeviceID + "). Error Code = " + result);
			}
			
			// Stop Cameras
			if(this.VideoCamera.IsRunning)
			{
				this.VideoCamera.Stop();
			}
			if(this.DepthCamera.IsRunning)
			{
				this.DepthCamera.Stop();
			}
			
			// Dispose of child instances
			this.LED = null;
			this.Motor = null;
			this.Accelerometer = null;
			this.VideoCamera = null;
			this.DepthCamera = null;
			
			// Unegister the device
			KinectNative.UnregisterDevice(this.devicePointer);
			
			// Not open anymore
			this.IsOpen = false;
		}
		
		/// <summary>
		/// Gets updated device status from the Kinect. This updates any properties in the 
		/// child devices (Motor, LED, etc.)
		/// </summary>
		public void UpdateStatus()
		{
			// Ask for new device status
			KinectNative.freenect_update_tilt_state(this.devicePointer);
			
			// Get updated device status
			IntPtr ptr = KinectNative.freenect_get_tilt_state(this.devicePointer);
			this.cachedDeviceState = (FreenectTiltState)Marshal.PtrToStructure(ptr, typeof(FreenectTiltState));
		}
		
		/// <summary>
		/// Shuts down the Kinect.NET library and closes any open devices.
		/// </summary>
		public static void Shutdown()
		{
			KinectNative.ShutdownContext();
		}
		
		/// <summary>
		/// Process any pending messages on the USB streams. This should be called every so often.
		/// </summary>
		public static void ProcessEvents()
		{
			KinectNative.freenect_process_events(KinectNative.Context);
		}
		
		/// <summary>
		/// Gets the number of Kinect devices connected 
		/// </summary>
		/// <remarks>
		/// This is just a support function for the Kinect.DeviceCount property
		/// </remarks>
		/// <returns>
		/// Number of Kinect devices connected.
		/// </returns>
		private static int GetDeviceCount()
		{		
			// Now we can just return w/e native method puts out
			return KinectNative.freenect_num_devices(KinectNative.Context);
		}
		
		/// <summary>
		/// Sets the logging level for the Kinect session. Support function for Kinect.LogLevel property.
		/// </summary>
		/// <param name="level">
		/// A <see cref="LogLevel"/>
		/// </param>
		private static void SetLogLevel(LogLevelOptions level)
		{
			KinectNative.freenect_set_log_level(KinectNative.Context, level);
			Kinect.logLevel = level;
		}
		
		/// <summary>
		/// Logging callback.
		/// </summary>
		/// <param name="device">
		/// A <see cref="IntPtr"/>
		/// </param>
		/// <param name="logLevel">
		/// A <see cref="Kinect.LogLevelOptions"/>
		/// </param>
		/// <param name="message">
		/// A <see cref="System.String"/>
		/// </param>
		internal static void LogCallback(IntPtr device, Kinect.LogLevelOptions logLevel, string message)
		{
			Kinect realDevice = KinectNative.GetDevice(device);
			Kinect.Log(null, new LogEventArgs(realDevice, logLevel, message));
		}
		
		/// <summary>
		/// Logging levels from the C library
		/// </summary>
		public enum LogLevelOptions
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
}

