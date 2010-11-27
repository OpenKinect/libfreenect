#ifndef KINECTWIN32
#define KINECTWIN32

#include <vector>
#include <windows.h>

namespace Kinect
{
	enum
	{
		KINECT_DEPTH_WIDTH = 640,
		KINECT_DEPTH_HEIGHT = 480,
		KINECT_COLOR_WIDTH = 640,
		KINECT_COLOR_HEIGHT = 480,
		KINECT_MICROPHONE_COUNT = 4,
		KINECT_AUDIO_BUFFER_LENGTH = 256	
	};

	class Kinect;
	
	enum
	{
		Led_Off = 0x0,
        Led_Green = 0x1,
        Led_Red = 0x2,
        Led_Yellow = 0x3,
        Led_BlinkingYellow = 0x4,
        Led_BlinkingGreen = 0x5,
        Led_AlternateRedYellow = 0x6,
        Led_AlternateRedGreen = 0x7
	};

	enum
	{
		RGB_mode_bayer,
		RGB_mode_IR
	};
	
	class KinectListener
	{
	public:
		virtual ~KinectListener(){};
		virtual void KinectDisconnected(Kinect *K) {};
		virtual void DepthReceived(Kinect *K) {};
		virtual void ColorReceived(Kinect *K) {};
		virtual void AudioReceived(Kinect *K) {};
	};

	class KinectInternalData;
	class Kinect
	{
	public:
		Kinect(void *internalhandle, void *internalmotorhandle);  // takes usb handle.. never explicitly construct! use kinectfinder!
		virtual ~Kinect();
		bool Opened();
		void SetMotorPosition(double pos);
		void SetLedMode(int NewMode);
		bool GetAcceleroData(float *x, float *y, float *z);
		
		void Run();
		void Stop();

		void SetRGBMode(int newmode);

		void AddListener(KinectListener *K);
		void RemoveListener(KinectListener *K);

		unsigned short mDepthBuffer[KINECT_DEPTH_WIDTH * KINECT_DEPTH_HEIGHT];
		unsigned char mColorBuffer[KINECT_COLOR_WIDTH * KINECT_COLOR_HEIGHT * 4];
		float mAudioBuffer[KINECT_MICROPHONE_COUNT][KINECT_AUDIO_BUFFER_LENGTH];
		
		std::vector<KinectListener *> mListeners;
		
		CRITICAL_SECTION mListenersLock;
		
		KinectInternalData* mInternalData;

		void KinectDisconnected();
		void DepthReceived();
		void ColorReceived();
		void AudioReceived();

		void ParseColorBuffer();
		void ParseColorBuffer32();
		
		void ParseIRBuffer();

		void ParseDepthBuffer();
	};

	class KinectFinder
	{
	public:

		KinectFinder();
		virtual ~KinectFinder();

		int GetKinectCount();
		Kinect *GetKinect( int index = 0, bool running = true );

		std::vector<Kinect *> mKinects;
	};
};

#endif