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

namespace freenect
{
	/// <summary>
	/// Abstract frame mode. Base class for Video and Depth frame modes. Not 
	/// to be used directly.
	/// </summary>
	public abstract class FrameMode
	{
		/// <summary>
		/// Gets the resolution setting for this mode (low, medium, high)
		/// </summary>
		public Resolution Resolution
		{
			get;
			private set;
		}
		
		/// <summary>
		/// Gets the total size of the frame in bytes
		/// </summary>
		public int Size
		{
			get;
			private set;
		}
		
		/// <summary>
		/// Gets the width of the frame in pixels
		/// </summary>
		public int Width
		{
			get;
			private set;
		}
		
		/// <summary>
		/// Gets the height of the frame in pixels
		/// </summary>
		public int Height
		{
			get;
			private set;
		}
		
		/// <summary>
		/// Gets the number of bits per pixel in the frame
		/// </summary>
		public int DataBitsPerPixel
		{
			get;
			private set;
		}
		
		/// <summary>
		/// Gets the number of pading bits per pixel in the frame
		/// </summary>
		public int PaddingBitsPerPixel
		{
			get;
			private set;
		}
		
		/// <summary>
		/// Gets the framerate in Hz that you can expect (sort of)
		/// </summary>
		public int FrameRate
		{
			get;
			private set;
		}
		
		/// <summary>
		/// Video format for this frame mode. This doesn't have to be valid
		/// as the frame mode could be for depth.
		/// </summary>
		protected VideoFormat videoFormat;
		
		/// <summary>
		/// Depth format for this frame mode. This doesn't have to be valid
		/// as the frame mode could be for video.
		/// </summary>
		protected DepthFormat depthFormat;
		
		/// <summary>
		/// Native mode struct that this managed one was spawned from
		/// </summary>
		internal FreenectFrameMode nativeMode;
		
		/// <summary>
		/// Gets a nice C# library version of the Frame Mode class
		/// </summary>
		/// <param name="mode">
		/// A <see cref="FreenectFrameMode"/>
		/// </param>
		/// <returns>
		/// A <see cref="FrameMode"/>
		/// </returns>
		internal static FrameMode FromInterop(FreenectFrameMode nativeMode, FrameModeType type)
		{
			FrameMode mode = null;
			
			// Make sure mode is valid
			if(nativeMode.IsValid == 0)
			{
				return null;
			}
			
			// Figure out what type of mode it is
			if(type == FrameMode.FrameModeType.VideoFormat)
			{
				mode = new VideoFrameMode();
			}
			else if(type == FrameMode.FrameModeType.DepthFormat)
			{
				mode = new DepthFrameMode();
			}
			
			// Copy over rest of data
			mode.nativeMode = nativeMode;
			mode.Size = nativeMode.Bytes;
			mode.Width = nativeMode.Width;
			mode.Height = nativeMode.Height;
			mode.Resolution = nativeMode.Resolution;
			mode.DataBitsPerPixel = nativeMode.DataBitsPerPixel;
			mode.PaddingBitsPerPixel = nativeMode.PaddingBitsPerPixel;
			mode.FrameRate = nativeMode.Framerate;
			mode.videoFormat = nativeMode.VideoFormat;
			mode.depthFormat = nativeMode.DepthFormat;
			
			return mode;
		}
		
		/// <summary>
		/// Gets a string representation of the FrameMode
		/// </summary>
		/// <returns>
		/// A <see cref="String"/>
		/// </returns>
		public override String ToString()
		{
			if(this is VideoFrameMode)
			{
				return this.Width + "x" + this.Height + " : " + this.videoFormat.ToString();
			}
			else
			{
				return this.Width + "x" + this.Height + " : " + this.depthFormat.ToString();
			}
		}
		
		/// <summary>
		/// Format mode type. This is only used interally. Don't touch!
		/// </summary>
		internal enum FrameModeType
		{
			VideoFormat,
			DepthFormat
		}
	}
}