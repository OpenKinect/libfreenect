#ifndef KINECTWIN32INTERNAL
#define KINECTWIN32INTERNAL
#include "Kinect-win32.h"
#include "libusb\include\usb.h"
namespace Kinect
{
	enum
	{

		RGB_NUM_XFERS = 30,		
		RGB_PKT_SIZE = 1920,
		RGB_PKTS_PER_XFER =32,

		//	RGB_XFER_SIZE = RGB_PKTS_PER_XFER*RGB_PKT_SIZE,

		DEPTH_NUM_XFERS = 10,
		DEPTH_PKT_SIZE = 1760,
		DEPTH_PKTS_PER_XFER =32,

		//	DEPTH_XFER_SIZE = DEPTH_PKTS_PER_XFER * DEPTH_PKT_SIZE ,

		USB_PKT_SIZE = 960

	};

	typedef unsigned char uint8_t;
	typedef unsigned short uint16_t;
	typedef unsigned int uint32_t;

	struct KinectUSBFrameHeader
	{
		uint8_t mMagic[2];
		uint8_t mPad;
		uint8_t mFlag;
		uint8_t mUnknown1;
		uint8_t mSequence;
		uint8_t mUnknown2;
		uint8_t mUnknown3;
		uint32_t mTimeStamp;
	};
	class KinectFrameInput;

	class KinectFrameInputCallbacks
	{
	public:

		virtual ~KinectFrameInputCallbacks(){};
		virtual void BufferComplete(KinectFrameInput *source) = 0;
	};

	class KinectFrameInput
	{
	public:

		KinectFrameInput(KinectFrameInputCallbacks *callbacks, usb_dev_handle* dev, unsigned char endpoint, int length_per_packet, int max_packets_in_buffer, int transfers_in_queue, int outputbuffersize);
		virtual ~KinectFrameInput();
		virtual bool CheckMagic(KinectUSBFrameHeader *header);
		
		virtual void ProcessPacket(KinectUSBFrameHeader *header, unsigned char *data, int datalen);
		
		int Reap();
		
		usb_dev_handle* mDeviceHandle; 
		int mEndPoint;
		int mMaxPacketLength; // expected bytes
		int mMaxActualPacketLength; // expected bytes rounded up to a multiple of USB_PKT_LENGTH 
		int mMaxPacketsPerBuffer;
		int mMaxTransfers;
		int mTransferSize;
		int mCurrentTransfer;
		bool mPacketStored;

		int mWriteHeadPosition;
		unsigned char mStartSequence;
		bool mDropThisFrame;
		unsigned char mCurrentSequence;

		KinectFrameInputCallbacks *mCallbacks;
		
		virtual	bool CheckSequence(KinectUSBFrameHeader *header);

		unsigned char mStoredPacket[960];
		
		unsigned char *mOutputBuffer;
		int mOutputBufferSize;
		void **mTransfers;
		unsigned char **mPacketBuffers;
	};

	class KinectInternalData: public KinectFrameInputCallbacks
	{
	public:
		KinectInternalData(Kinect *inParent);
		~KinectInternalData();

		void LockDepth(){EnterCriticalSection(&depth_lock);};
		void UnlockDepth(){LeaveCriticalSection(&depth_lock);};
		void LockRGB(){EnterCriticalSection(&rgb_lock);};
		void UnlockRGB(){LeaveCriticalSection(&rgb_lock);};
		
		void LockDepthThread(){EnterCriticalSection(&depththread_lock);};
		void UnlockDepthThread(){LeaveCriticalSection(&depththread_lock);};
		void LockRGBThread(){EnterCriticalSection(&rgbthread_lock);};
		void UnlockRGBThread(){LeaveCriticalSection(&rgbthread_lock);};

		void SetMotorPosition(double newpos);
		void SetLedMode(unsigned short NewMode);
		bool GetAcceleroData(float *x, float *y, float *z);

		virtual void BufferComplete(KinectFrameInput *source);

		int mErrorCount; 

		CRITICAL_SECTION rgb_lock;
		CRITICAL_SECTION depth_lock;
		CRITICAL_SECTION rgbthread_lock;
		CRITICAL_SECTION depththread_lock;
		
		usb_dev_handle *mDeviceHandle;
		usb_dev_handle *mDeviceHandle_Motor;
		usb_dev_handle *mDeviceHandle_Audio;
		Kinect *mParent;

		KinectFrameInput *mDepthInput;
		KinectFrameInput *mRGBInput;

		void OpenDevice(usb_device_t *dev,usb_device_t *motordev);
		void CloseDevice();

		void WriteCameraRegisters();
		
		uint8_t *depth_sourcebuf2;
		uint8_t *rgb_buf2;
		unsigned short mCommandTag;
		bool Running;
		bool RGBRunning;
		bool DepthRunning;
		bool ThreadDone;
		
		void ErrorMessage(const char *fmt, ...){};
		void TraceMessage(const char *fmt, ...){};
		void WarningMessage(const char *fmt, ...){};
		
		void RunThread();
		void StopThread();
		int mRGBMode;
		void SetRGBMode(int newmode);

		int WriteRegister(unsigned short registeridx, unsigned short value);
		int SendCommand(unsigned short command, unsigned char *commandbuffer, unsigned int commandbufferlength, unsigned char *replybuffer, unsigned int replybufferlength);
	};
};
#endif


