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
		/// Current data format
		/// </summary>
		private DataFormatOption dataFormat;
		
		/// <summary>
		/// Direct access data buffer for the video camera
		/// </summary>
		private IntPtr dataBuffer = IntPtr.Zero;
		
		/// <summary>
		/// DepthMap waiting for data
		/// </summary>
		private DepthMap nextFrameDepthMap = null;
		
		/// <summary>
		/// Event raised when video data (an image) has been received.
		/// </summary>
		public event DataReceivedEventHandler DataReceived = delegate { };

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
		/// Gets or sets the data format this camera will send depth data in.
		/// </summary>
		/// <value>
		/// Gets or sets the 'dataFormat' member
		/// </value>
		public DataFormatOption DataFormat
		{
			get
			{
				return this.dataFormat;
			}
			set
			{
				this.SetDataFormat(value);
			}
		}
		
		/// <summary>
		/// Gets sizes in bytes for a frame in each of the formats supported by the depth camera.
		/// </summary>
		public static DataFormatSizeCollection DataFormatSizes
		{
			get;
			private set;
		}
		
		/// <summary>
		/// Gets dimensions for a frame for each of the formats supported by the depth camera.
		/// </summary>
		public static DataFormatDimensionCollection DataFormatDimensions
		{
			get;
			private set;
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
			
			// Set format to 11 bit by default
			this.DataFormat = DataFormatOption.Format11Bit;
			
			// Setup callbacks
			KinectNative.freenect_set_depth_callback(parent.devicePointer, DepthCallback);
		}
		
		/// <summary>
		/// Static constructor
		/// </summary>
		static DepthCamera()
		{
			DepthCamera.DataFormatSizes = new DepthCamera.DataFormatSizeCollection();
			DepthCamera.DataFormatDimensions = new DepthCamera.DataFormatDimensionCollection();
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
		/// Sets the DepthCameras's data format. Support function for DepthCamera.DataFormat
		/// </summary>
		/// <param name="format">
		/// A <see cref="DepthCamera.DataFormatOptions"/>
		/// </param>
		private void SetDataFormat(DepthCamera.DataFormatOption format)
		{
			// change depth map that's waiting cause format has changed
			this.UpdateNextFrameDepthMap();
			
			int result = KinectNative.freenect_set_depth_format(this.parentDevice.devicePointer, format);
			if(result != 0)
			{
				throw new Exception("Could not switch to depth format " + format + ". Error Code: " + result);
			}
			this.dataFormat = format;
		}
		
		/// <summary>
		/// Updates the next frame depth map that's waiting for data with any state changes
		/// </summary>
		protected void UpdateNextFrameDepthMap()
		{
			if(this.DataBuffer == IntPtr.Zero)
			{
				// have to set our own buffer as the depth buffer
				this.nextFrameDepthMap = new DepthMap(this.DataFormat);
				KinectNative.freenect_set_depth_buffer(this.parentDevice.devicePointer, this.nextFrameDepthMap.DataPointer);
			}
			else	
			{
				// already have a buffer from user
				this.nextFrameDepthMap = new DepthMap(this.DataFormat, this.DataBuffer);
			}
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
		/// Format for Depth data coming in
		/// </summary>
		public enum DataFormatOption
		{
			Format11Bit = 0,
			Format10Bit = 1,
			FormatPacked11Bit = 2,
			FormatPacked10Bit = 3
		}
		
		/// <summary>
		/// Format dimensions
		/// </summary>
		public class DataFormatDimensionCollection
		{
			/// <summary>
			/// Map of sizes
			/// </summary>
			private Dictionary<DataFormatOption, Point> dimensions;
			
			/// <summary>
			/// Gets the dimensions of the specified format
			/// </summary>
			/// <param name="format">
			/// Format to get the size for
			/// </param>
			public Point this[DataFormatOption format]
			{
				get
				{
					return this.dimensions[format];
				}
			}
			
			/// <summary>
			/// constructor
			/// </summary>
			public DataFormatDimensionCollection()
			{
				this.dimensions = new Dictionary<DataFormatOption, Point>();
				this.dimensions.Add(DataFormatOption.Format11Bit, new Point(640, 480));
				this.dimensions.Add(DataFormatOption.Format10Bit, new Point(640, 480));
				this.dimensions.Add(DataFormatOption.FormatPacked11Bit, new Point(640, 480));
				this.dimensions.Add(DataFormatOption.FormatPacked10Bit, new Point(640, 480));
			}
		}
		
		/// <summary>
		/// Format sizes
		/// </summary>
		public class DataFormatSizeCollection
		{
			/// <summary>
			/// Map of sizes
			/// </summary>
			private Dictionary<DataFormatOption, int> sizes;
			
			/// <summary>
			/// Gets the size of the specified format
			/// </summary>
			/// <param name="format">
			/// Format to get the size for
			/// </param>
			public int this[DataFormatOption format]
			{
				get
				{
					return this.sizes[format];
				}
			}
			
			/// <summary>
			/// constructor
			/// </summary>
			public DataFormatSizeCollection()
			{
				this.sizes = new Dictionary<DataFormatOption, int>();
				this.sizes.Add(DataFormatOption.Format11Bit, 614400);
				this.sizes.Add(DataFormatOption.Format10Bit, 614400);
				this.sizes.Add(DataFormatOption.FormatPacked11Bit, 422400);
				this.sizes.Add(DataFormatOption.FormatPacked10Bit, 384000);
			}
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

