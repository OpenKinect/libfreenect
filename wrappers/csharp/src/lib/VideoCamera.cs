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
	/// Provides access to the RGB/IR camera on the Kinect
	/// </summary>
	///
	/// 
	public class VideoCamera : BaseCamera
	{		
		/// <summary>
		/// Gets and sets the current video mode for the video camera. For best results, 
		/// use one of the modes in the VideoCamera.Modes collection.
		/// </summary>
		public VideoFrameMode Mode
		{
			get
			{
				return (VideoFrameMode)this.captureMode;
			}
			set
			{
				this.SetVideoMode(value);
			}
		}
		
		/// <summary>
		/// Gets a list of available, valid video modes
		/// </summary>
		public VideoFrameMode[] Modes
		{
			get;
			private set;
		}
		
		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="parent">
		/// Parent <see cref="Kinect"/> device that this video camera is part of
		/// </param>
		internal VideoCamera(Kinect parent) : base(parent)
		{
			// Update modes available for this video camera
			this.UpdateVideoModes();
			
			// Use the first mode by default
			this.Mode = this.Modes[0];
			
			// Setup callbacks
			KinectNative.freenect_set_video_callback(parent.devicePointer, this.DataCallback);
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
			
			// All done
			this.IsRunning = true;
		}
		
		/// <summary>
		/// Stops streaming video data from this camera
		/// </summary>
		public void Stop()
		{	
			if(this.IsRunning == false)
			{
				// Not running, nothing to do
				return;
			}
			
			// Stop camera
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
		protected override void SetDataBuffer(IntPtr ptr)
		{
			// Save data buffer
			this.dataBuffer = ptr;
			
			// Tell the kinect library about it
			KinectNative.freenect_set_video_buffer(this.parentDevice.devicePointer, ptr);
			
			// update image map
			this.UpdateNextFrameImageMap();
		}
		
		/// <summary>
		/// Sets the current video mode
		/// </summary>
		/// <param name="mode">
		/// Video mode to switch to.
		/// </param>
		protected void SetVideoMode(VideoFrameMode mode)
		{
			// Is this a different mode?
			if(this.Mode == mode)
			{
				return;	
			}
			
			// Stop camera first if running
			bool running = this.IsRunning;
			if(running)
			{	
				this.Stop();
			}
			
			// Check to make sure mode is valid by finding it again
			VideoFrameMode foundMode = VideoFrameMode.Find(mode.Format, mode.Resolution);
			if(foundMode == null)
			{
				throw new Exception("Invalid Video Mode: [" + mode.Format + ", " + mode.Resolution + "]");
			}
			
			// Save mode
			this.captureMode = mode;
			
			// All good, switch to new mode
			int result = KinectNative.freenect_set_video_mode(this.parentDevice.devicePointer, foundMode.nativeMode);
			if(result != 0)
			{
				throw new Exception("Mode switch failed. Error Code: " + result);
			}
			
			// Update image map
			this.UpdateNextFrameImageMap();
			
			// If we were running before, start up again
			if(running)
			{
				this.Start();	
			}
		}
		
		/// <summary>
		/// Updates the next frame imagemap that's waiting for data with any state changes
		/// </summary>
		protected void UpdateNextFrameImageMap()
		{
			if(this.DataBuffer == IntPtr.Zero)
			{
				// have to set our own buffer as the video buffer
				this.nextFrameData = new ImageMap(this.Mode);
			}
			else	
			{
				// already have a buffer from user
				this.nextFrameData = new ImageMap(this.Mode, this.DataBuffer);
			}
			
			// Set video buffer
			KinectNative.freenect_set_video_buffer(this.parentDevice.devicePointer, this.nextFrameData.DataPointer);
		}
		
		/// <summary>
		/// Updates list of video modes that this camera has.
		/// </summary>
		private void UpdateVideoModes()
		{
			List<VideoFrameMode> modes = new List<VideoFrameMode>();
			
			// Get number of modes
			int numModes = KinectNative.freenect_get_video_mode_count(this.parentDevice.devicePointer);
			
			// Go through modes
			for(int i = 0; i < numModes; i++)
			{
				VideoFrameMode mode = (VideoFrameMode)FrameMode.FromInterop(KinectNative.freenect_get_video_mode(i), FrameMode.FrameModeType.VideoFormat);
				if(mode != null)
				{
					modes.Add(mode);
				}
			}
			
			// All done
			this.Modes = modes.ToArray();
		}
		
	}
	
}