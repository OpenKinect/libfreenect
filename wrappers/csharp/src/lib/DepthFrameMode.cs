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
	/// Depth frame mode.
	/// </summary>
	public class DepthFrameMode : FrameMode
	{
		/// <summary>
		/// Gets the format for this depth frame mode
		/// </summary>
		public DepthFormat Format
		{
			get
			{
				return this.depthFormat;	
			}
		}
		
		/// <summary>
		/// Ninja constructor
		/// </summary>
		internal DepthFrameMode()
		{
			
		}
		
		/// <summary>
		/// Finds a mode, given a format and resolution.
		/// </summary>
		/// <param name="format">
		/// Depth format for the mode
		/// </param>
		/// <param name="resolution">
		/// Resolution for the mode
		/// </param>
		/// <returns>
		/// Mode with the format/resolution combo. Null if the combination is invalid.
		/// </returns>
		public static DepthFrameMode Find(DepthFormat format, Resolution resolution)
		{
			return (DepthFrameMode)FrameMode.FromInterop(KinectNative.freenect_find_depth_mode(resolution, format), FrameMode.FrameModeType.DepthFormat);
		}
		
	}
}