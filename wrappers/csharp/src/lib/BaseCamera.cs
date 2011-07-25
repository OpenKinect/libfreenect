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
using System.ComponentModel;

namespace freenect
{
	/// <summary>
	/// Base camera class. Provides access to Kinect cameras.
	/// </summary>
	public abstract class BaseCamera
	{
		/// <summary>
		/// Parent Kinect instance
		/// </summary>
		protected Kinect parentDevice;
		
		/// <summary>
		/// Current capture mode
		/// </summary>
		protected FrameMode captureMode;
		
		/// <summary>
		/// Direct access data buffer for the camera
		/// </summary>
		protected IntPtr dataBuffer = IntPtr.Zero;
		
		/// <summary>
		/// Data map waiting for data
		/// </summary>
		protected BaseDataMap nextFrameData = null;
		
		/// <summary>
		/// Callback (delegate) for camera data
		/// </summary>
		protected FreenectCameraDataCallback DataCallback;
		
		/// <summary>
		/// Event raised when data (an image) has been received.
		/// </summary>
		public event DataReceivedEventHandler DataReceived = delegate { };
		
		/// <summary>
		/// Gets whether this camera is streaming data
		/// </summary>
		public bool IsRunning
		{
			get;
			protected set;
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
		/// Base camera constructor
		/// </summary>
		/// <param name="parent">
		/// A <see cref="Kinect"/>
		/// </param>
		internal BaseCamera(Kinect parent)
		{
			// Save parent device
			this.parentDevice = parent;
			
			// Not running by default
			this.IsRunning = false;
			
			// Create callback
			this.DataCallback = new FreenectCameraDataCallback(this.HandleDataReceived);
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
		protected void HandleDataReceived(IntPtr device, IntPtr imageData, UInt32 timestamp)
		{			
			// Calculate datetime from timestamp
			DateTime dateTime = new System.DateTime(1970, 1, 1, 0, 0, 0, 0).AddSeconds(timestamp);
			
			// Send out event
			this.DataReceived(this, new DataReceivedEventArgs(dateTime, this.nextFrameData));
		}
		
		/// <summary>
		/// Sets the direct access buffer for the Camera.
		/// </summary>
		/// <param name="ptr">
		/// Pointer to the direct access data buffer for the Camera.
		/// </param>
		protected abstract void SetDataBuffer(IntPtr ptr);
		
		/// <summary>
		/// Delegate for camera data events
		/// </summary>
		public delegate void DataReceivedEventHandler(object sender, DataReceivedEventArgs e);
		
		/// <summary>
		/// Event data for camera data received events
		/// </summary>
		public class DataReceivedEventArgs
		{
			/// <summary>
			/// Gets the timestamp for this data
			/// </summary>
			public DateTime Timestamp
			{
				get;
				private set;
			}
			
			/// <summary>
			/// Gets... the data
			/// </summary>
			public BaseDataMap Data
			{
				get;
				private set;
			}
			
			public DataReceivedEventArgs(DateTime timestamp, BaseDataMap b)
			{
				this.Timestamp = timestamp;
				this.Data = b;
			}
		}
	}
	
}