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

namespace LibFreenect
{
	/// <summary>
	/// Delegate for Kinect.Log event
	/// </summary>
	public delegate void LogEventHandler(object sender, LogEventArgs e);
	
	/// <summary>
	/// Log event data
	/// </summary>
	/// <author>Aditya Gaddam (adityagaddam@gmail.com)</author>
	/// 
	public class LogEventArgs
	{
		/// <summary>
		/// Gets the Kinect device this log item originated from
		/// </summary>
		public Kinect Device
		{
			get;
			set;
		}
		
		/// <summary>
		/// Gets the timestamp for this log item. This is done on the C# 
		/// side and does not mean it was SENT EXACTLY at the specified 
		/// time from the Kinect low level library.
		/// </summary>
		public DateTime Timestamp
		{
			get;
			private set;
		}
		
		/// <summary>
		/// Gets the logging level the library it set to
		/// </summary>
		public Kinect.LogLevelOptions LogLevel
		{
			get;
			private set;
		}
		
		/// <summary>
		/// Gets the log item text
		/// </summary>
		public string Message
		{
			get;
			private set;
		}
		
		public LogEventArgs(Kinect device, Kinect.LogLevelOptions logLevel, string message)
		{
			this.Device = device;
			this.LogLevel = logLevel;
			this.Message = message;
		}
		
	}
}

