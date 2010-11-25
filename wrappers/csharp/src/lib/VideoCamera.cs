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

namespace LibFreenect
{
	/// <summary>
	/// Provides access to the RGB/IR camera on the Kinect
	/// </summary>
	/// <author>Aditya Gaddam (adityagaddam@gmail.com)</author>
	/// 
	public class VideoCamera
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
		/// ImageMap waiting for data
		/// </summary>
		private ImageMap nextFrameImage = null;
		
		/// <summary>
		/// Event raised when video data (an image) has been received.
		/// </summary>
		public event DataReceivedEventHandler DataReceived = delegate { };
		
		/// <summary>
		/// Gets whether the video camera is streaming data
		/// </summary>
		public bool IsRunning
		{
			get;
			private set;
		}
		
		/// <summary>
		/// Gets or sets the data format this camera will send images in.
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
		/// Gets sizes in bytes for a frame in each of the formats supported by the video camera.
		/// </summary>
		public static DataFormatSizeCollection DataFormatSizes
		{
			get;
			private set;
		}
		
		/// <summary>
		/// Gets dimensions for a frame for each of the formats supported by the video camera.
		/// </summary>
		public static DataFormatDimensionCollection DataFormatDimensions
		{
			get;
			private set;
		}
		
		/// <summary>
		/// Gets or sets the direct data buffer the USB stream will use for 
		/// the video camera. This should be a pinned location in memory. 
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
		/// Static constructor
		/// </summary>
		static VideoCamera()
		{
			VideoCamera.DataFormatSizes = new VideoCamera.DataFormatSizeCollection();
			VideoCamera.DataFormatDimensions = new VideoCamera.DataFormatDimensionCollection();
		}
		
		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="parent">
		/// Parent <see cref="Kinect"/> device that this video camera is part of
		/// </param>
		internal VideoCamera(Kinect parent)
		{
			// Save parent device
			this.parentDevice = parent;
			
			// Not running by default
			this.IsRunning = false;
			
			// Set format to RGB by default
			this.DataFormat = DataFormatOption.RGB;
			
			// Setup callbacks
			KinectNative.freenect_set_video_callback(parent.devicePointer, new FreenectVideoDataCallback(VideoCamera.HandleDataReceived));
		}
		
		/// <summary>
		/// Starts streaming RGB data from this camera
		/// </summary>
		public void Start()
		{
			// Update image map before starting
			this.UpdateNextFrameImageMap();
			
			// Start
			int result = KinectNative.freenect_start_video(this.parentDevice.devicePointer);
			if(result != 0)
			{
				throw new Exception("Could not start video stream. Error Code: " + result);
			}
			this.IsRunning = true;
		}
		
		/// <summary>
		/// Stops streaming video data from this camera
		/// </summary>
		public void Stop()
		{
			int result = KinectNative.freenect_stop_video(this.parentDevice.devicePointer);
			if(result != 0)
			{
				throw new Exception("Could not stop video stream. Error Code: " + result);
			}
			this.IsRunning = false;
		}
		
		/// <summary>
		/// Sets the direct access buffer for the VideoCamera.
		/// </summary>
		/// <param name="ptr">
		/// Pointer to the direct access data buffer for the VideoCamera.
		/// </param>
		protected void SetDataBuffer(IntPtr ptr)
		{
			// Save data buffer
			this.dataBuffer = ptr;
			
			// Tell the kinect library about it
			KinectNative.freenect_set_video_buffer(this.parentDevice.devicePointer, ptr);
			
			// update image map
			this.UpdateNextFrameImageMap();
		}
		
		/// <summary>
		/// Sets the VideoCamera's data format. Support function for VideoCamera.DataFormat property.
		/// </summary>
		/// <param name="format">
		/// Format to change the video camera to
		/// </param>
		protected void SetDataFormat(VideoCamera.DataFormatOption format)
		{
			// change imagemap that's waiting cause format has changed
			this.UpdateNextFrameImageMap();
			
			// change format
			int result = KinectNative.freenect_set_video_format(this.parentDevice.devicePointer, format);
			if(result != 0)
			{
				throw new Exception("Could not switch to video format " + format + ". Error Code: " + result);
			}
			this.dataFormat = format;
		}
		
		/// <summary>
		/// Updates the next frame imagemap that's waiting for data with any state changes
		/// </summary>
		protected void UpdateNextFrameImageMap()
		{
			if(this.DataBuffer == IntPtr.Zero)
			{
				// have to set our own buffer as the video buffer
				this.nextFrameImage = new ImageMap(this.DataFormat);
				KinectNative.freenect_set_video_buffer(this.parentDevice.devicePointer, this.nextFrameImage.DataPointer);
			}
			else	
			{
				// already have a buffer from user
				this.nextFrameImage = new ImageMap(this.DataFormat, this.DataBuffer);
			}
		}
		
		/// <summary>
		/// Handles image data from teh video camera
		/// </summary>
		/// <param name="device">
		/// A <see cref="IntPtr"/>
		/// </param>
		/// <param name="imageData">
		/// A <see cref="IntPtr"/>
		/// </param>
		/// <param name="timestamp">
		/// A <see cref="UInt32"/>
		/// </param>
		private static void HandleDataReceived(IntPtr device, IntPtr imageData, UInt32 timestamp)
		{
			// Figure out which device actually got this frame
			Kinect realDevice = KinectNative.GetDevice(device);
			
			// Calculate datetime from timestamp
			DateTime dateTime = new System.DateTime(1970, 1, 1, 0, 0, 0, 0).AddSeconds(timestamp);
			
			// Send out event
			realDevice.VideoCamera.DataReceived(realDevice, new DataReceivedEventArgs(dateTime, realDevice.VideoCamera.nextFrameImage));
		}
		
		/// <summary>
		/// Format for VideoCamera data coming in
		/// </summary>
		public enum DataFormatOption
		{
			RGB 			= 0,
			Bayer 			= 1,
			IR8Bit 			= 2,
			IR10Bit 		= 3,
			IR10BitPacked 	= 4
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
				this.dimensions.Add(DataFormatOption.RGB, new Point(640, 480));
				this.dimensions.Add(DataFormatOption.Bayer, new Point(640, 480));
				this.dimensions.Add(DataFormatOption.IR8Bit, new Point(640, 488));
				this.dimensions.Add(DataFormatOption.IR10Bit, new Point(640, 488));
				this.dimensions.Add(DataFormatOption.IR10BitPacked, new Point(640, 488));
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
				this.sizes.Add(DataFormatOption.RGB, 921600);
				this.sizes.Add(DataFormatOption.Bayer, 307200);
				this.sizes.Add(DataFormatOption.IR8Bit, 312320);
				this.sizes.Add(DataFormatOption.IR10Bit, 614400);
				this.sizes.Add(DataFormatOption.IR10BitPacked, 390400);
			}
		}
		
		/// <summary>
		/// Delegate for video camera data events
		/// </summary>
		public delegate void DataReceivedEventHandler(object sender, DataReceivedEventArgs e);
		
		/// <summary>
		/// Video camera data
		/// </summary>
		public class DataReceivedEventArgs
		{
			/// <summary>
			/// Gets the timestamp for this image
			/// </summary>
			public DateTime Timestamp
			{
				get;
				private set;
			}
			
			/// <summary>
			/// Gets image data
			/// </summary>
			public ImageMap Image
			{
				get;
				private set;
			}
			
			public DataReceivedEventArgs(DateTime timestamp, ImageMap b)
			{
				this.Timestamp = timestamp;
				this.Image = b;
			}
		}
		
	}
	
}