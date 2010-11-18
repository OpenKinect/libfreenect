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
            KinectMotor motor = new KinectMotor();

            if( args.Length ==1 )
            {
                sbyte pos = sbyte.Parse(args[0]);
                motor.SetTilt(pos);
            }else{
                ExerciseMotor(motor,+50);
                ExerciseMotor(motor,-50);
                ExerciseMotor(motor,+60);
                ExerciseMotor(motor,-60);
                ExerciseMotor(motor, 0);
            }

            Console.WriteLine("done");
            Console.ReadKey();
        }

        private static void ExerciseMotor(KinectMotor motor, sbyte pos)
        {

            motor.SetLED(KinectLEDStatus.AlternateRedYellow);
            motor.SetTilt(pos);
            Thread.Sleep(TimeSpan.FromSeconds(2));
            motor.SetLED(KinectLEDStatus.Green);
        }
    }
}
