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
using System.Collections.Generic;
using System.Drawing;
using System.Runtime.InteropServices;

namespace freenect
{
	/// <summary>
	/// Represents a map of rgb values from the RGBCamera
	/// </summary>
	///
	/// 
	public class ImageMap
	{	
		/// <summary>
		/// GC handle to the data in the image map
		/// </summary>
		private GCHandle dataHandle;
	
		/// <summary>
		/// Gets the width of the image map
		/// </summary>
		public int Width
		{
			get;
			private set;
		}
		
		/// <summary>
		/// Gets the height of the image map
		/// </summary>
		public int Height
		{
			get;
			private set;
		}
		
		/// <summary>
		/// Gets the raw data in the ImageMap. This data is in a 1-dimensional 
		/// array so it's easy to work with in unsafe code. 
		/// </summary>
		public byte[] Data
		{
			get;
			private set;
		}
		
		/// <summary>
		/// Gets the data pointer from the Kinect library
		/// </summary>
		public IntPtr DataPointer
		{
			get;
			private set;
		}
		
		/// <summary>
		/// Gets the mode in which this image was captured.
		/// </summary>
		public VideoFrameMode CaptureMode
		{
			get;
			private set;
		}
		
		/// <summary>
		/// Constructor with only mode speicifed. Data is allocated inside.
		/// </summary>
		/// <param name="mode">
		/// A <see cref="VideoFrameMode"/>
		/// </param>
		internal ImageMap(VideoFrameMode mode)
		{
			// Save format and resolution
			this.Width = mode.Width;
			this.Height = mode.Height;
			this.CaptureMode = mode;
			this.Data = new byte[mode.Size];
			this.dataHandle = GCHandle.Alloc(this.Data, GCHandleType.Pinned);
			this.DataPointer = this.dataHandle.AddrOfPinnedObject();
		}
		
		/// <summary>
		/// Constructor with only mode and data pointer specified. No allocation is made.
		/// </summary>
		/// <param name="mode">
		/// A <see cref="VideoFrameMode"/>
		/// </param>
		internal ImageMap(VideoFrameMode mode, IntPtr bufferPointer)
		{
			this.Width = mode.Width;
			this.Height = mode.Height;
			this.CaptureMode = mode;
			this.Data = null;
			this.dataHandle = default(GCHandle);
			this.DataPointer = bufferPointer;
		}
		
		/// <summary>
		/// Destructoooorrr
		/// </summary>
		~ImageMap()
		{
			if(this.dataHandle != default(GCHandle))
			{
				this.dataHandle.Free();
			}
		}
	}
}

