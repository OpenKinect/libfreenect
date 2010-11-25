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
using System.Threading;
using LibFreenect;

namespace ConsoleTest
{
	/// <summary>
	/// Performs some basic tests on the Kinect device connected to see
	/// if the wrapper is functioning properly
	/// </summary>
	/// <author>Aditya Gaddam (adityagaddam@gmail.com)</author>
	class MainClass
	{
		/// <summary>
		/// Driver
		/// </summary>
		/// <param name="args">
		/// A <see cref="System.String[]"/>
		/// </param>
		public static void Main (string[] args)
		{
			Console.WriteLine("----------------------------------------");
			Console.WriteLine("| Kinect.NET Wrapper Test              |");
			Console.WriteLine("----------------------------------------\n");
			
			// Try to get number of devices connected
			Console.WriteLine(" - Device Count: " + Kinect.DeviceCount);
			
			// Do more tests if there are devices present
			if(Kinect.DeviceCount > 0)
			{
				// Try to open a device
				Kinect k = new Kinect(0);
				Console.Write(" - Opening device 0...");
				k.Open();
				Console.WriteLine("Done.");
				
				// Try to set LED colors
				Console.WriteLine(" - LED Testing");
				string[] colors = Enum.GetNames(typeof(KinectLED.ColorOption));
				foreach(string color in colors)
				{
					var c = (KinectLED.ColorOption)Enum.Parse(typeof(KinectLED.ColorOption), color);
					Console.WriteLine("\t - Setting LED to Color: " + color);
					k.LED.Color = c;
					Thread.Sleep(3000);
				}
				
				// Try to control motor
				Console.WriteLine(" - Motor Testing");
				Console.WriteLine("\t - Setting tilt to 1 (should face all the way up)");
				k.Motor.Tilt = 1;
				Thread.Sleep(3000);
				Console.WriteLine("\t - Setting tilt to -1 (should face all the way down)");
				k.Motor.Tilt = -1;
				Thread.Sleep(3000);
				Console.WriteLine("\t - Setting tilt to 0 (should be back level)");
				k.Motor.Tilt = 0;
				Thread.Sleep(3000);
				
				// Close device
				Console.Write(" - Closing device 0...");
				k.Close();
				Console.WriteLine("Done.");
			}
			
			// Shutdown the Kinect context
			Kinect.Shutdown();
		}
	}
}

