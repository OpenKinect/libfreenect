using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using LibUsbDotNet.Main;
using LibUsbDotNet;
using OpenKinect;
using System.Threading;

namespace OpenKinectConsole
{
    class OpenKinectConsole
    {
        static void Main(string[] args)
        {
            ExerciseMotor();

            
            // Wait for user input..
            Console.WriteLine("done");
            Console.ReadKey();

        }

        private static void ExerciseMotor()
        {
            KinectMotor motor = new KinectMotor();

            motor.SetTilt(50);
            motor.SetLED(KinectLEDStatus.Red);
            Thread.Sleep(TimeSpan.FromSeconds(2));

            motor.SetTilt(255);
            motor.SetLED(KinectLEDStatus.BlinkingYellow);
            Thread.Sleep(TimeSpan.FromSeconds(2));

            motor.SetTilt(128);
            motor.SetLED(KinectLEDStatus.AlternateRedGreen);
            Thread.Sleep(TimeSpan.FromSeconds(2));

            motor.SetLED(KinectLEDStatus.Green);
        }
    }
}
