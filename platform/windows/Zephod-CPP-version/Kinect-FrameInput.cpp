#include "Kinect-win32-internal.h"

namespace Kinect
{
	KinectFrameInput::KinectFrameInput(KinectFrameInputCallbacks *callbacks, usb_dev_handle* dev, unsigned char endpoint, int length_per_packet, int max_packets_in_buffer, int transfers_in_queue, int outputbuffersize)
	{
		mCallbacks = callbacks;
		mEndPoint = endpoint;
		mDropThisFrame = true;
		mDeviceHandle = dev;
		mMaxActualPacketLength = 1920;
		mMaxPacketLength = length_per_packet;
		mMaxPacketsPerBuffer = max_packets_in_buffer;
		mMaxTransfers  = transfers_in_queue;
		mTransferSize = mMaxPacketsPerBuffer*mMaxActualPacketLength;
		mOutputBufferSize = outputbuffersize;		
		mOutputBuffer = new unsigned char[outputbuffersize];
		mTransfers = new void *[mMaxTransfers];
		mPacketBuffers = new unsigned char *[mMaxTransfers];
		mPacketStored = false;
		mCurrentTransfer = 0;
		mWriteHeadPosition = 0;

		for (int i = 0;i<mMaxTransfers;i++)
		{
			int	ret = usb_isochronous_setup_async(mDeviceHandle, &mTransfers[i], mEndPoint, mMaxActualPacketLength);
			if (ret<0)
			{
				printf("error setting up isochronous request!");	
			};

			mPacketBuffers[i] = new unsigned char[mTransferSize];
			ZeroMemory(mPacketBuffers[i], mTransferSize);
		};
		
		for (int i = 0;i<mMaxTransfers;i++)
		{
			int ret = usb_submit_async(mTransfers[i], (char*)&mPacketBuffers[i][0], mTransferSize);
			if (ret<0)
			{
				printf("error submitting isochronous request!");	
			};
		};
	};

	KinectFrameInput::~KinectFrameInput()
	{
		for (int i = 0;i<mMaxTransfers;i++)
		{
			usb_cancel_async(mTransfers[i]);
			usb_free_async(&mTransfers[i]);
			mTransfers[i] = NULL;
		}

		if (mPacketBuffers)
		{
			for (int i = 0;i<mMaxTransfers;i++)
			{
				if (mPacketBuffers[i]) delete mPacketBuffers[i];
			};
			delete [] mPacketBuffers;
		};

		if (mOutputBuffer) delete [] mOutputBuffer;
		if (mTransfers) delete [] mTransfers;		
	};

	bool KinectFrameInput::CheckMagic(KinectUSBFrameHeader *header)
	{
		if (header->mMagic[0] == 'R' && header->mMagic[1] == 'B') return true;
		return false;;
	};
		
	void KinectFrameInput::ProcessPacket(KinectUSBFrameHeader *header, unsigned char *data, int datalen)
	{
		bool	b_valid = CheckSequence(header);
		switch (header->mFlag&0x0f)
		{
		default:
			return;

		case 0x01:
			{
				mDropThisFrame = false;
				mStartSequence = header->mSequence;				
				memcpy(mOutputBuffer, data, datalen);
				mWriteHeadPosition = datalen;
				return;
			};
		case 0x02:
			{		
				if( b_valid )
				{
					int BytesToCopy = __min(datalen, mOutputBufferSize-mWriteHeadPosition);
					if (BytesToCopy > 0)
					{
						memcpy(mOutputBuffer+mWriteHeadPosition, data, BytesToCopy);
						mWriteHeadPosition += BytesToCopy;
					}
				}
				else
				{
					mDropThisFrame = true;
				}
				return;
			};
		case 0x05:
			{				
				int BytesToCopy = __min(datalen, mOutputBufferSize-mWriteHeadPosition);
				if (BytesToCopy > 0)
				{
					memcpy(mOutputBuffer+mWriteHeadPosition, data, BytesToCopy);
					mWriteHeadPosition += BytesToCopy;
				};
				if (!mDropThisFrame)
				{
					if (mCallbacks) mCallbacks->BufferComplete(this);
					mDropThisFrame = true;
				}
				else
				{
//					printf("frame dropped.. missing %d bytes!\n", mOutputBufferSize-mWriteHeadPosition);
				};
				return;
			};
		};
	};
		
	int KinectFrameInput::Reap()
	{
		int UsbStatus = usb_reap_async_nocancel(mTransfers[mCurrentTransfer], 10000);
		int RetVal = 0;
		if (UsbStatus>0)
		{
			RetVal = 1;
			unsigned char *CurrentBuffer = &mPacketBuffers[mCurrentTransfer][0];
			int PacketOffset = 0;
			int PacketStart = 0;	
			int LeftOverBytes = mTransferSize;
			int MaxSubPackets = mTransferSize / USB_PKT_SIZE;
			
			while (PacketStart<MaxSubPackets)
			{
				KinectUSBFrameHeader *Header = (KinectUSBFrameHeader*) &CurrentBuffer[PacketStart*USB_PKT_SIZE];
				if (CheckMagic(Header))
				{
					break;
				}
				else
				{
					PacketStart++;
				}
			}
						
			if (PacketStart>0)
			{
				PacketOffset  = PacketStart*USB_PKT_SIZE;
				LeftOverBytes-= PacketOffset;				
			};
			
			while (LeftOverBytes >0)
			{
				int CurrentPacketLength = __min(LeftOverBytes,mMaxActualPacketLength);
				KinectUSBFrameHeader *Header = (KinectUSBFrameHeader*)&CurrentBuffer[PacketOffset];
				if (CheckMagic(Header))
				{
					if (CurrentPacketLength<mMaxPacketLength)
					{
						if (CurrentPacketLength == 960)
						{
							printf ("valid halve packet found!\n");
						//	ProcessPacket(Header, &CurrentBuffer[PacketOffset+sizeof(KinectUSBFrameHeader)], __min(mMaxPacketLength,CurrentPacketLength)-sizeof(KinectUSBFrameHeader));
						}
						else
						{
							printf ("incomplete packet of length %d found..\n", CurrentPacketLength);
						};
					}
					else
					{
						ProcessPacket(Header, &CurrentBuffer[PacketOffset+sizeof(KinectUSBFrameHeader)], __min(mMaxPacketLength,CurrentPacketLength)-sizeof(KinectUSBFrameHeader));
					}
				}
				else
				{
					CurrentPacketLength = 960;
				}
				PacketOffset += CurrentPacketLength;
				LeftOverBytes-= CurrentPacketLength;
			};			
		}
		else
		{
			if (UsbStatus<0)
			{
				if (UsbStatus == -116 )
				{
					// timeout = not ready yet!
					return 0;
				}
				usb_cancel_async(mTransfers[mCurrentTransfer]);
			}
		};
		ZeroMemory(&mPacketBuffers[mCurrentTransfer][0], mTransferSize);
		
		int ret = usb_submit_async(mTransfers[mCurrentTransfer], (char*)&mPacketBuffers[mCurrentTransfer][0],  mTransferSize);
		if( ret < 0 )
		{
			printf("error submitting async usb request: %s\n", usb_strerror());
			usb_cancel_async(mTransfers[mCurrentTransfer]);
		}
		mCurrentTransfer = (mCurrentTransfer + 1) % mMaxTransfers;
	
		return RetVal;
	};

	bool KinectFrameInput::CheckSequence(KinectUSBFrameHeader *header)
	{
		unsigned char NewSequence = mCurrentSequence + 1;
		mCurrentSequence = header->mSequence;
		if (header->mSequence != NewSequence)
		{
			//printf("sequence lost: expected %02x, got %02x.\n", NewSequence, header->mSequence);
			return false;
		}
		return true;
	};
};