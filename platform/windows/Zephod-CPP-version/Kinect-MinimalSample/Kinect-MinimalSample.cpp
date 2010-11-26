#include <stdio.h>
#include <conio.h> // for _kbhit and _getch 
#include "../Kinect-win32.h"

// The "Kinect" Project has been added to the project dependencies of this project. 


// the listener callback object. Implement these methods to do your own processing
class Listener: public Kinect::KinectListener
{
public:

		virtual void KinectDisconnected(Kinect::Kinect *K) 
		{
			printf("Kinect disconnected!\n");
		};
		
		// Depth frame reception complete. this only means the transfer of 1 frame has succeeded. 
		// No data conversion/parsing will be done until you call "ParseDepthBuffer" on the kinect 
		// object. This is to prevent needless processing in the wrong thread.
		virtual void DepthReceived(Kinect::Kinect *K) 
		{
			K->ParseDepthBuffer();						
			
			// K->mDepthBuffer is now valid and usable!
			// see Kinect-Demo.cpp for a more complete example on what to do with this buffer
		};
		
		// Color frame reception complete. this only means the transfer of 1 frame has succeeded. 
		// No data conversion/parsing will be done until you call "ParseColorBuffer" on the kinect 
		// object. This is to prevent needless processing in the wrong thread.
		virtual void ColorReceived(Kinect::Kinect *K) 
		{
			K->ParseColorBuffer();
		
			// K->mColorBuffer is now valid and usable!
			// see Kinect-Demo.cpp for a more complete example on what to do with this buffer
		};
		
		// not functional yet:
		virtual void AudioReceived(Kinect::Kinect *K) {};
};

int main(int argc, char **argv)
{
	Kinect::KinectFinder KF;
	if (KF.GetKinectCount() < 1)
	{
		printf("Unable to find Kinect devices... Is one connected?\n");
		return 0;
	}

	Kinect::Kinect *K = KF.GetKinect();
	if (K == 0)
	{
		printf("error getting Kinect...\n");
		return 0;
	};
	
	// create a new Listener instance
	Listener *L = new Listener();
	
	// register the listener with the kinect. Make sure you remove the 
	// listener before deleting the instance! A good place to unregister 
	// would be your listener destructor.
	K->AddListener(L);

	// SetMotorPosition accepts 0 to 1 range
	K->SetMotorPosition(1);
	
	// Led mode ranges from 0 to 7, see the header for possible values
	K->SetLedMode(Kinect::Led_Yellow);
	
	// Grab 10 accelerometer values from the kinect
	float x,y,z;
	for (int i =0 ;i<10;i++)
	{
		if (K->GetAcceleroData(&x,&y,&z))
		{
			printf("accelerometer reports: %f,%f,%f\n", x,y,z);
		}
		Sleep(5);
	};

	printf("press any key to quit...");
	while (!_kbhit())
	{
		Sleep(5);
	};
	_getch();
	
	// remove and delete the listener instance
	K->RemoveListener(L);
	delete L;
	
	//turn the led off
	K->SetLedMode(Kinect::Led_Off);
	
	// when the KinectFinder instance is destroyed, it will tear down and free all kinects.
	return 0;
};

