#include "Kinect-win32.h"
#include "Kinect-win32-internal.h"

#include<algorithm>

namespace Kinect
{
	KinectFinder::KinectFinder()
	{
		usb_init();
		usb_find_busses();
		usb_find_devices();

		usb_bus *CurrentBus = usb_get_busses();

		std::vector< void *> KinectsFound;
		std::vector< void *> KinectMotorsFound;
		std::vector< void *> KinectAudioFound;

		while (CurrentBus)
		{
			struct usb_device * CurrentDev = CurrentBus->devices;
			while (CurrentDev)
			{
				if (CurrentDev->descriptor.idVendor == 0x045E &&
					CurrentDev->descriptor.idProduct == 0x02AE) // cam = 0x02AE  motor = 0x02B0  audio = 0x02AD
				{
					KinectsFound.push_back(CurrentDev);
				};
				if (CurrentDev->descriptor.idVendor == 0x045E &&
					CurrentDev->descriptor.idProduct == 0x02B0) // cam = 0x02AE  motor = 0x02B0  audio = 0x02AD
				{
					KinectMotorsFound.push_back(CurrentDev);
				};
				if (CurrentDev->descriptor.idVendor == 0x045E &&
					CurrentDev->descriptor.idProduct == 0x02AD) // cam = 0x02AE  motor = 0x02B0  audio = 0x02AD
				{
					KinectAudioFound.push_back(CurrentDev);
				};
				CurrentDev = CurrentDev->next;
			};
			CurrentBus = CurrentBus->next;
		};


		for (unsigned int i = 0;i<KinectsFound.size();i++)
		{
			void *Motor = NULL;
			if (i<KinectMotorsFound.size()) Motor = KinectMotorsFound[i];
			Kinect *K = new Kinect(KinectsFound[i], Motor);
			if (K->Opened())
			{
				mKinects.push_back(K);
			}
			else
			{
				delete K;
			}

		};


	};

	KinectFinder::~KinectFinder()
	{
		for (unsigned int i = 0;i<mKinects.size();i++)
		{
			delete mKinects[i];
		};
		mKinects.clear();
	};

	Kinect *KinectFinder::GetKinect(int index, bool running)
	{
		if (index>-1 && index< (int)mKinects.size())
		{
			Kinect* kinect = mKinects[index];
			if( running )
				kinect->Run();
			return kinect;
		}
		return NULL;
	};

	int KinectFinder::GetKinectCount()
	{
		return mKinects.size();
	};

	bool Kinect::Opened()
	{
		if (mInternalData->mDeviceHandle) return true;
		return false;
	};

	Kinect::Kinect(void *internaldata, void *internalmotordata)
	{
		InitializeCriticalSection(&mListenersLock);
		mInternalData = new KinectInternalData(this);
		mInternalData->OpenDevice( (struct usb_device *)internaldata, (struct usb_device *)internalmotordata );
	};

	Kinect::~Kinect()
	{
		if (mInternalData) 
		{
			delete mInternalData;
		}
	};

	void Kinect::KinectDisconnected()
	{
		EnterCriticalSection(&mListenersLock);
		for (unsigned int i=0;i<mListeners.size();i++) mListeners[i]->KinectDisconnected(this);
		LeaveCriticalSection(&mListenersLock);
	};

	void Kinect::DepthReceived()
	{		
		EnterCriticalSection(&mListenersLock);
		for (unsigned int i=0;i<mListeners.size();i++) mListeners[i]->DepthReceived(this);
		LeaveCriticalSection(&mListenersLock);
	};

	void Kinect::ColorReceived()
	{	
		EnterCriticalSection(&mListenersLock);
		for (unsigned int i=0;i<mListeners.size();i++) mListeners[i]->ColorReceived(this);
		LeaveCriticalSection(&mListenersLock);
	};

	void Kinect::Run()
	{
		mInternalData->RunThread();
	};

	void Kinect::Stop()
	{
		mInternalData->StopThread();
	};

	void Kinect::AddListener(KinectListener *KL)
	{
		EnterCriticalSection(&mListenersLock);
		if (KL) mListeners.push_back(KL);
		LeaveCriticalSection(&mListenersLock);
	};

	void Kinect::RemoveListener(KinectListener *KL)
	{
		EnterCriticalSection(&mListenersLock);
		std::vector<KinectListener*>::iterator f = find(mListeners.begin(), mListeners.end(), KL);
		if (f!= mListeners.end()) mListeners.erase(f);
		LeaveCriticalSection(&mListenersLock);
	};

	void Kinect::AudioReceived()
	{
		EnterCriticalSection(&mListenersLock);
		for (unsigned int i=0;i<mListeners.size();i++) mListeners[i]->AudioReceived(this);
		LeaveCriticalSection(&mListenersLock);
	};

	void Kinect::ParseColorBuffer32() 
	{
		// Maa optimized
		//  faster but as bad as the previous one
		//  done in 2 by 2 block here
		//  destination pixel should be made by using several source pixel
		KinectInternalData *KID = mInternalData;

		KID->LockRGB();
		uint8_t* dst = mColorBuffer;
		uint8_t* src = KID->rgb_buf2 - 1;
		uint8_t  v;

		for (int y=480; y>0; y-=2 ) 
		{
			for (int x=320; x>0; --x ) 
			{
				//green
				v = *++src;
				*(dst+1) = v;
				*(dst+5) = v;
				*(dst+3) = 0xff; //alpha
				dst += 4;
				//red
				v = *++src;
				*(dst+2)  = v;
				*(dst-2)  = v;
				*(dst+640*4+2) = v;
				*(dst+640*4-2) = v;
				*(dst+3) = 0xff; //alpha
				dst += 4;
			}
			for (int x=320; x>0; --x ) 
			{
				//blue
				v = *++src;
				*(dst)   = v;
				*(dst-4)  = v;
				*(dst-640*4) = v;
				*(dst-640*4-4) = v;
				*(dst+3) = 0xff; //alpha
				dst += 4;
				//green
				v = *++src;
				*(dst+1) = v;
				*(dst-3) = v;
				*(dst+3) = 0xff; //alpha
				dst += 4;
			}
		}
		KID->UnlockRGB();
	}

	static inline void convert_packed_to_16bit (uint8_t * raw, uint16_t * frame, int vw)
	{
		int mask = (1 << vw) - 1;
		int i;
		int bitshift = 0;
		for (i = 0; i < (640 * 480); i++)
		{
			int idx = (i * vw) / 8;
			uint32_t word = (raw[idx] << (16)) | (raw[idx + 1] << 8) | raw[idx + 2];
			frame[i] = ((word >> (((3 * 8) - vw) - bitshift)) & mask);
			bitshift = (bitshift + vw) % 8;
		}
	}


	void Kinect::ParseColorBuffer()
	{
		KinectInternalData *KID = mInternalData;
		
		if (KID->mRGBMode == RGB_mode_IR)
		{
			unsigned short *buf = new unsigned short [640*480];
			KID->LockRGB();
			convert_packed_to_16bit(KID->rgb_buf2, buf, 10);
			KID->UnlockRGB();
			unsigned char *out = mColorBuffer;
			unsigned short *IR = buf;
			for (int y=0; y<480; y++) 
			{
				for (int x=0; x<640; x++) 
				{	
					unsigned short cur = *IR++;
					*out++ = cur &0xff;
					*out++ = cur>>8;
					*out++ = 0;
				};
			};
			delete [] buf;
			
		}
		else
		{
			KID->LockRGB();

			for (int y=1; y<479; y++) 
			{
				for (int x=0; x<640; x++) 
				{
					int i = y*640+x;
					if (x&1) 
					{
						if (y&1) 
						{
							mColorBuffer[3*i+1] = KID->rgb_buf2[i];
							mColorBuffer[3*i+4] = KID->rgb_buf2[i];
						} 
						else 
						{
							mColorBuffer[3*i] = KID->rgb_buf2[i];
							mColorBuffer[3*i+3] = KID->rgb_buf2[i];
							mColorBuffer[3*(i-640)] = KID->rgb_buf2[i];
							mColorBuffer[3*(i-640)+3] = KID->rgb_buf2[i];
						}
					} 
					else 
					{
						if (y&1) 
						{
							mColorBuffer[3*i+2] = KID->rgb_buf2[i];
							mColorBuffer[3*i-1] = KID->rgb_buf2[i];
							mColorBuffer[3*(i+640)+2] = KID->rgb_buf2[i];
							mColorBuffer[3*(i+640)-1] = KID->rgb_buf2[i];
						}
						else 
						{
							mColorBuffer[3*i+1] = KID->rgb_buf2[i];
							mColorBuffer[3*i-2] = KID->rgb_buf2[i];
						}
					}
				}
			}
			KID->UnlockRGB();
		};
	}

	void Kinect::ParseDepthBuffer()
	{
		if (0)
		{
			/*KinectInternalData *KID = mInternalData;
			int bitshift = 0;
			KID->LockDepth();
			for (int i=0; i<640*480; i++) 
			{
			int i2 = i / 2;
			FILE *F = fopen("dump.raw", "wb+");
			if (F)

			{
			fwrite(KID->depth_sourcebuf2, 1, 640*480*2, F);
			fclose(F);
			};
			mDepthBuffer[i] = KID->depth_sourcebuf2[i2*2]+(KID->depth_sourcebuf2[i2*2+1]<<8);
			}
			KID->UnlockDepth();*/
		}
		else
		{
			KinectInternalData *KID = mInternalData;
			KID->LockDepth();

#if 1
			//	Maa's version should be way faster
			uint8_t		a;	
			uint8_t		b;
			uint8_t		c;
			uint8_t*	p = (uint8_t*) (KID->depth_sourcebuf2);
			unsigned short*	dst = mDepthBuffer;
			--p;
			--dst;
			for( int i=(640*480)/8; i>0; --i )
			{
				a = *++p;
				b = *++p;
				*++dst = (a<<3) | (b>>5);
				a = *++p;
				*++dst = ( (b & 0x1f) <<6) | (a>>2);
				b = *++p;
				c = *++p;
				*++dst = ( (a & 0x3)  <<9) | (b<<1) | (c>>7);
				b = *++p;
				*++dst = ( (c & 0x7f) <<4) | (b>>4);
				a = *++p;
				*++dst = ( (b & 0xf)  <<7) | (a>>1);
				b = *++p;
				c = *++p;
				*++dst = ( (a & 0x1)  <<10) | (b<<2) | (c>>6);
				b = *++p;
				*++dst = ( (c & 0x3f) <<5) | (b>>3);
				a = *++p;
				*++dst = ( (b & 0x7)  <<8) | a;
			}
#else
			int bitshift = 0;
			for (int i=0; i<640*480; i++) 
			{
				int idx = (i*11)/8;
				uint32_t word = (KID->depth_sourcebuf2[idx]<<16) | (KID->depth_sourcebuf2[idx+1]<<8) | KID->depth_sourcebuf2[idx+2];
				mDepthBuffer[i] = ((word >> (13-bitshift)) & 0x7ff);
				bitshift = (bitshift + 11) % 8;
			}
#endif
			KID->UnlockDepth();
		};
	};

	void Kinect::SetMotorPosition(double newpos)
	{
		mInternalData->SetMotorPosition(newpos);
	};

	void Kinect::SetLedMode(int NewMode)
	{
		mInternalData->SetLedMode(NewMode);
	};


	bool Kinect::GetAcceleroData(float *x, float *y, float *z)
	{
		if (x && y && z)
		{
			return mInternalData->GetAcceleroData(x,y,z);
		};
		return false;
	};

	void Kinect::SetRGBMode(int newmode)
	{
		mInternalData->SetRGBMode(newmode);
	};
};