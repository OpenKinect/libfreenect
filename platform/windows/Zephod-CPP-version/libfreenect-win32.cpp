#include "libfreenect.h"
#include "Kinect-win32.h"
#include <vector>
#include <algorithm>
class FreenectListener;

struct _freenect_context
{
	Kinect::KinectFinder mFinder;
	freenect_log_cb mLogCallback;
	freenect_loglevel mLogLevel;
	std::vector<freenect_device*> mDevices;
};

struct _freenect_device
{
	Kinect::Kinect *mKinect;
	_freenect_context *mContext;
	void *mUserData;	
	freenect_depth_cb mDepthCallback;
	freenect_rgb_cb mRGBCallback;
	bool mDepthEnabled;
	bool mRGBEnabled;
	int mDepthFrameCounter;
	int mLastDepthFrameCounter;
	int mRGBFrameCounter;
	int mLastRGBFrameCounter;
	FreenectListener *mListener;
	CRITICAL_SECTION mRGBEnableLock;
	CRITICAL_SECTION mDepthEnableLock;
};

class FreenectListener: public Kinect::KinectListener
{
public:
	_freenect_device *mFreenectDev;

	virtual ~FreenectListener()
	{
	};

	FreenectListener(_freenect_device *owner)
	{
		mFreenectDev = owner;
	}
	
	virtual void DepthReceived(Kinect::Kinect *K) 
	{
		EnterCriticalSection(&mFreenectDev->mDepthEnableLock);
		if (mFreenectDev->mDepthEnabled)
		{
			mFreenectDev->mDepthFrameCounter++;			
		};
		LeaveCriticalSection(&mFreenectDev->mDepthEnableLock);
	};
	
	virtual void ColorReceived(Kinect::Kinect *K) 
	{
		EnterCriticalSection(&mFreenectDev->mRGBEnableLock);
		if (mFreenectDev->mRGBEnabled)
		{
			mFreenectDev->mRGBFrameCounter++;			
		};
		LeaveCriticalSection(&mFreenectDev->mRGBEnableLock);
	};		
};

int __cdecl freenect_init(freenect_context **ctx, freenect_usb_context *usb_ctx)
{
	_freenect_context *context = new _freenect_context;
	context->mLogCallback = NULL;
	context->mLogLevel = FREENECT_LOG_FATAL;
	*ctx = context;
	return 0;
};

__declspec(dllexport) int __cdecl freenect_shutdown(freenect_context *ctx)
{
	if (ctx)
	{
		delete ctx;
	};
	return 0;
};

__declspec(dllexport) void __cdecl freenect_set_log_level(freenect_context *ctx, freenect_loglevel level)
{
	if (ctx) ctx->mLogLevel = level;
};

__declspec(dllexport) void __cdecl freenect_set_log_callback(freenect_context *ctx, freenect_log_cb cb)
{	
	if (ctx) ctx->mLogCallback = cb;
};

__declspec(dllexport) int __cdecl freenect_process_events(freenect_context *ctx)
{
	if (ctx)
	{
		for (unsigned int i = 0;i<ctx->mDevices.size();i++)
		{
			freenect_device *dev = ctx->mDevices[i];
			if (dev->mDepthEnabled && dev->mDepthCallback)
			{
				if (dev->mDepthFrameCounter != dev->mLastDepthFrameCounter)
				{
					dev->mKinect->ParseDepthBuffer();
					dev->mDepthCallback(dev, dev->mKinect->mDepthBuffer, dev->mDepthFrameCounter);
					dev->mLastDepthFrameCounter = dev->mDepthFrameCounter;
				};
			};
			if (dev->mRGBEnabled && dev->mRGBCallback)
			{
				if (dev->mRGBFrameCounter != dev->mLastRGBFrameCounter)
				{
					dev->mKinect->ParseColorBuffer();
					dev->mRGBCallback(dev, dev->mKinect->mColorBuffer, dev->mRGBFrameCounter);
					dev->mLastRGBFrameCounter = dev->mRGBFrameCounter;
				};
			};
		};
	};
	return 0;
};

__declspec(dllexport) int __cdecl freenect_num_devices(freenect_context *ctx)
{
	if (ctx) return ctx->mFinder.GetKinectCount(); 
	return 0;
};

__declspec(dllexport) int __cdecl freenect_open_device(freenect_context *ctx, freenect_device **dev, int index)
{
	if (ctx)
	{
		if (index >= 0 && index < ctx->mFinder.GetKinectCount()) 
		{
			_freenect_device *device = new _freenect_device;
			ZeroMemory(device, sizeof(_freenect_device));
			device->mKinect = ctx->mFinder.GetKinect(index, true);
			InitializeCriticalSection(&device->mDepthEnableLock);
			InitializeCriticalSection(&device->mRGBEnableLock);
			device->mListener = new FreenectListener(device);
			device->mContext = ctx;
			device->mKinect->AddListener(device->mListener);
			ctx->mDevices.push_back(device);
			*dev = device;
		};
	};
	return 0;
};

__declspec(dllexport) int __cdecl freenect_close_device(freenect_device *dev)
{
	if (dev)
	{
		if (dev->mKinect)
		{
			dev->mKinect->Stop();

			if (dev->mListener)
			{
				EnterCriticalSection(&dev->mDepthEnableLock);
				dev->mDepthEnabled = false;
				LeaveCriticalSection(&dev->mDepthEnableLock);
				
				EnterCriticalSection(&dev->mRGBEnableLock);
				dev->mRGBEnabled = false;
				LeaveCriticalSection(&dev->mRGBEnableLock);
				
				dev->mKinect->RemoveListener(dev->mListener);
				delete dev->mListener;
			};
		};

		if (dev->mKinect)
		{
			dev->mKinect = NULL;
		};
		
		DeleteCriticalSection(&dev->mRGBEnableLock);
		DeleteCriticalSection(&dev->mDepthEnableLock);
		std::vector<freenect_device *>::iterator f = find(dev->mContext->mDevices.begin(), dev->mContext->mDevices.end(), dev);
		if (f!= dev->mContext->mDevices.end()) dev->mContext->mDevices.erase(f);
		delete dev;
	};
	return 0;
};

__declspec(dllexport) void __cdecl freenect_set_user(freenect_device *dev, void *user)
{
	if (dev) dev->mUserData = user;
};

__declspec(dllexport) void *__cdecl freenect_get_user(freenect_device *dev)
{
	if (dev) return dev->mUserData;
	return 0;
};

__declspec(dllexport) void __cdecl freenect_set_depth_callback(freenect_device *dev, freenect_depth_cb cb)
{
	if (dev)
	{
		dev->mDepthCallback = cb;
	};
};
__declspec(dllexport) void __cdecl freenect_set_rgb_callback(freenect_device *dev, freenect_rgb_cb cb)
{
	if (dev)
	{
		dev->mRGBCallback = cb;
	};
};

__declspec(dllexport) int __cdecl freenect_set_rgb_format(freenect_device *dev, freenect_rgb_format fmt)
{
	return 0;
};

__declspec(dllexport) int __cdecl freenect_set_depth_format(freenect_device *dev, freenect_depth_format fmt)
{
	return 0;
};

__declspec(dllexport) int __cdecl freenect_start_depth(freenect_device *dev)
{
	if (dev) dev->mDepthEnabled = true;
	return 0;
};

__declspec(dllexport) int __cdecl freenect_start_rgb(freenect_device *dev)
{
	if (dev) dev->mRGBEnabled = true;
	return 0;
};

__declspec(dllexport) int __cdecl freenect_stop_depth(freenect_device *dev)
{
	if (dev) dev->mDepthEnabled = false;
	return 0;
};

__declspec(dllexport) int __cdecl freenect_stop_rgb(freenect_device *dev)
{
	if (dev) dev->mRGBEnabled = false;
	return 0;
};

__declspec(dllexport) int __cdecl freenect_set_tilt_degs(freenect_device *dev, double angle)
{
	if (dev)
	{
		dev->mKinect->SetMotorPosition(angle);
	};
	return 0;
};

__declspec(dllexport) int __cdecl freenect_set_led(freenect_device *dev, freenect_led_options option)
{
	if (dev)
	{
		dev->mKinect->SetLedMode(option);
	};
	return 0;
};

__declspec(dllexport) int __cdecl freenect_get_raw_accel(freenect_device *dev, int16_t* x, int16_t* y, int16_t* z)
{
	return 0;
};
__declspec(dllexport) int __cdecl freenect_get_mks_accel(freenect_device *dev, double* x, double* y, double* z)
{
	return 0;
}


