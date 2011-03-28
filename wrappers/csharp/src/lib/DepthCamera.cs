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
using System.Collections.Generic;
using System.Drawing;

namespace freenect
{
	/// <summary>
	/// Provides access to the depth camera on the Kinect
	/// </summary>
	///
	/// 
	public class DepthCamera
	{
		
		/// <summary>
		/// Parent Kinect instance
		/// </summary>
		private Kinect parentDevice;
		
		/// <summary>
		/// Current depth camera mode
		/// </summary>
		private DepthFrameMode depthMode;
		
		/// <summary>
		/// Direct access data buffer for the depth camera
		/// </summary>
		private IntPtr dataBuffer = IntPtr.Zero;
		
		/// <summary>
		/// DepthMap waiting for data
		/// </summary>
		private DepthMap nextFrameDepthMap = null;
		
		/// <summary>
		/// Event raised when depth data has been received.
		/// </summary>
		public event DataReceivedEventHandler DataReceived = delegate { };

		/// <summary>
		/// Callback (delegate) for depth data
		/// </summary>
		private FreenectDepthDataCallback DepthCallback = new FreenectDepthDataCallback(DepthCamera.HandleDataReceived);

		/// <summary>
		/// Gets whether the depth camera is streaming data
		/// </summary>
		public bool IsRunning
		{
			get;
			private set;
		}
		
		/// <summary>
		/// Gets or sets the depth camera's mode.
		/// </summary>
		public DepthFrameMode Mode
		{
			get
			{
				return this.depthMode;
			}
			set
			{
				this.SetDepthMode(value);
			}
		}		
		
		/// <summary>
		/// Gets or sets the direct data buffer the USB stream will use for 
		/// the depth camera. This should be a pinned location in memory. 
		/// If set to IntPtr.Zero, the library will manage the data buffer 
		/// for you.
		/// </summary>
		public IntPtr DataBuffer
		{
			get
			{
				return this.dataBuffer;
			}
			set
			{
				this.SetDataBuffer(value);
			}
		}
		
		/// <summary>
		/// List of available, valid modes for the depth camera
		/// </summary>
		public DepthFrameMode[] Modes
		{
			get;
			private set;
		}
		
		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="parent">
		/// Parent <see cref="Kinect"/> device that this depth camera is part of
		/// </param>
		internal DepthCamera(Kinect parent)
		{
			// Save parent device
			this.parentDevice = parent;
			
			// Not running by default
			this.IsRunning = false;
			
			// Update lsit of available modes for this camera
			this.UpdateDepthModes();
			
			// Set the mode to the first available mode
			this.Mode = this.Modes[0];
			
			// Setup callbacks
			KinectNative.freenect_set_depth_callback(parent.devicePointer, DepthCallback);
		}
		
		/// <summary>
		/// Starts streaming depth data from this camera
		/// </summary>
		public void Start()
		{
			// Update depth map before starting
			this.UpdateNextFrameDepthMap();
			
			// Start
			int result = KinectNative.freenect_start_depth(this.parentDevice.devicePointer);
			if(result != 0)
			{
				throw new Exception("Could not start depth stream. Error Code: " + result);
			}
			this.IsRunning = true;
		}
		
		/// <summary>
		/// Stops streaming depth data from this camera
		/// </summary>
		public void Stop()
		{
			int result = KinectNative.freenect_stop_depth(this.parentDevice.devicePointer);
			if(result != 0)
			{
				throw new Exception("Could not depth RGB stream. Error Code: " + result);
			}
			this.IsRunning = false;
		}
		
		/// <summary>
		/// Sets the direct access buffer for the DepthCamera.
		/// </summary>
		/// <param name="ptr">
		/// Pointer to the direct access data buffer for the DepthCamera.
		/// </param>
		protected void SetDataBuffer(IntPtr ptr)
		{
			// Save data buffer
			this.dataBuffer = ptr;
			
			// Tell the kinect library about it
			KinectNative.freenect_set_depth_buffer(this.parentDevice.devicePointer, ptr);
			
			// update depth map
			this.UpdateNextFrameDepthMap();
		}
		
		/// <summary>
		/// Sets the current depth camera mode
		/// </summary>
		/// <param name="mode">
		/// Depth camera mode to switch to.
		/// </param>
		protected void SetDepthMode(DepthFrameMode mode)
		{
			// Check to make sure mode is valid by finding it again
			DepthFrameMode foundMode = DepthFrameMode.Find(mode.Format, mode.Resolution);
			if(foundMode == null)
			{
				throw new Exception("Invalid Depth Camera Mode: [" + mode.Format + ", " + mode.Resolution + "]");
			}
			
			// Save mode
			this.depthMode = mode;
			
			// All good, switch to new mode
			int result = KinectNative.freenect_set_depth_mode(this.parentDevice.devicePointer, foundMode.nativeMode);
			if(result != 0)
			{
				throw new Exception("Mode switch failed. Error Code: " + result);
			}
			
			// Update depth map
			this.UpdateNextFrameDepthMap();
		}
		
		/// <summary>
		/// Updates list of depth modes that this camera has.
		/// </summary>
		private void UpdateDepthModes()
		{
			List<DepthFrameMode> modes = new List<DepthFrameMode>();
			
			// Get number of modes
			int numModes = KinectNative.freenect_get_depth_mode_count(this.parentDevice.devicePointer);
			
			// Go through modes
			for(int i = 0; i < numModes; i++)
			{
				DepthFrameMode mode = (DepthFrameMode)FrameMode.FromInterop(KinectNative.freenect_get_depth_mode(i), FrameMode.FrameModeType.DepthFormat);
				if(mode != null)
				{
					modes.Add(mode);
				}
			}
			
			// All done
			this.Modes = modes.ToArray();
		}
		
		/// <summary>
		/// Updates the next frame depth map that's waiting for data with any state changes
		/// </summary>
		protected void UpdateNextFrameDepthMap()
		{
			if(this.DataBuffer == IntPtr.Zero)
			{
				// have to set our own buffer as the depth buffer
				this.nextFrameDepthMap = new DepthMap(this.Mode);
			}
			else	
			{
				// already have a buffer from user
				this.nextFrameDepthMap = new DepthMap(this.Mode, this.DataBuffer);
			}
			
			// Set new buffer at library level;
			KinectNative.freenect_set_depth_buffer(this.parentDevice.devicePointer, this.nextFrameDepthMap.DataPointer);
		}
		
		/// <summary>
		/// Static callback for C function. This finds out which device the callback was meant for 
		/// and calls that specific DepthCamera's depth data handler.
		/// </summary>
		/// <param name="device">
		/// A <see cref="IntPtr"/>
		/// </param>
		/// <param name="depthData">
		/// A <see cref="IntPtr"/>
		/// </param>
		/// <param name="timestamp">
		/// A <see cref="Int32"/>
		/// </param>
		private static void HandleDataReceived(IntPtr device, IntPtr depthData, UInt32 timestamp)
		{
			// Figure out which device actually got this frame
			Kinect realDevice = KinectNative.GetDevice(device);
			
			// Calculate datetime from timestamp
			DateTime dateTime = new System.DateTime(1970, 1, 1, 0, 0, 0, 0).AddSeconds(timestamp);
			
			// Send out event
			realDevice.DepthCamera.DataReceived(realDevice, new DataReceivedEventArgs(dateTime, realDevice.DepthCamera.nextFrameDepthMap));
		}
		
		/// <summary>
		/// Delegate for depth camera data events
		/// </summary>
		public delegate void DataReceivedEventHandler(object sender, DataReceivedEventArgs e);
		
		/// <summary>
		/// Depth camera data
		/// </summary>
		public class DataReceivedEventArgs
		{
			/// <summary>
			/// Gets the timestamp for when this depth data was received
			/// </summary>
			public DateTime Timestamp
			{
				get;
				private set;
			}
			
			/// <summary>
			/// Gets the depth data 
			/// </summary>
			public DepthMap DepthMap
			{
				get;
				private set;
			}
			
			/// <summary>
			/// constructor
			/// </summary>
			/// <param name="timestamp">
			/// A <see cref="DateTime"/>
			/// </param>
			/// <param name="depthMap">
			/// A <see cref="DepthMap"/>
			/// </param>
			public DataReceivedEventArgs(DateTime timestamp, DepthMap depthMap)
			{
				this.Timestamp = timestamp;
				this.DepthMap = depthMap;
			}
		}
	}
}

