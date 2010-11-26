/* USB Benchmark for libusb-win32

 Copyright © 2010 Travis Robinson. <libusbdotnet@gmail.com>
 website: http://sourceforge.net/projects/libusb-win32
 
 This program is free software; you can redistribute it and/or modify it
 under the terms of the GNU Lesser General Public License as published by 
 the Free Software Foundation; either version 2 of the License, or 
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful, but 
 WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 License for more details.
 
 You should have received a copy of the GNU Lesser General Public License
 along with this program; if not, please visit www.gnu.org.
*/

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>

#include "usb.h"

#define _BENCHMARK_VER_ONLY
#include "benchmark_rc.rc"

#define MAX_OUTSTANDING_TRANSFERS 10

// This is used only in VerifyData() for display information
// about data validation mismatches.
#define CONVDAT(format,...) printf("[data-mismatch] " format,__VA_ARGS__)

// All output is directed through these macros.
//
#define CONERR(format,...) printf("err : [" __FUNCTION__ "] " format, __VA_ARGS__)
#define CONMSG(format,...) printf(format,__VA_ARGS__)
#define CONWRN(format,...) printf("wrn : "format,__VA_ARGS__)
#define CONDBG(format,...) printf("dbg : "format,__VA_ARGS__)

#define CONERR0(message) CONERR("%s", message)
#define CONMSG0(message) CONMSG("%s", message)
#define CONWRN0(message) CONWRN("%s", message)
#define CONDBG0(message) CONDBG("%s", message)

// This is the libusb-win32 return code for a transfer that timed out.
#define TRANSFER_TIMEDOUT -116

// Custom vendor requests that must be implemented in the benchmark firmware.
// Test selection can be bypassed with the "notestselect" argument.
//
enum BENCHMARK_DEVICE_COMMANDS
{
    SET_TEST = 0x0E,
    GET_TEST = 0x0F,
};

// Tests supported by the official benchmark firmware.
//
enum BENCHMARK_DEVICE_TEST_TYPE
{
    TestTypeNone	= 0x00,
    TestTypeRead	= 0x01,
    TestTypeWrite	= 0x02,
    TestTypeLoop	= TestTypeRead|TestTypeWrite,
};

// This software was mainly created for testing the libusb-win32 kernel & user driver.
enum BENCHMARK_TRANSFER_MODE
{
	// Tests for the libusb-win32 sync transfer function.
    TRANSFER_MODE_SYNC,

	// Test for async function, iso transfers, and queued transfers
    TRANSFER_MODE_ASYNC,
};

// Holds all of the information about a test.
struct BENCHMARK_TEST_PARAM
{
    // User configurable value set from the command line.
    //
    INT Vid;			// Vendor ID
    INT Pid;			// Porduct ID
    INT Intf;			// Interface number
    INT Ep;				// Endpoint number (1-15)
    INT Refresh;		// Refresh interval (ms)
    INT Timeout;		// Transfer timeout (ms)
    INT Retry;			// Number for times to retry a timed out transfer before aborting
    INT BufferSize;		// Number of bytes to transfer
    INT BufferCount;	// Number of outstanding asynchronous transfers
    BOOL NoTestSelect;	// If true, don't send control message to select the test type.
    BOOL UseList;		// Show the user a device list and let them choose a benchmark device. 
	INT IsoPacketSize; // Isochronous packet size (defaults to the endpoints max packet size)
    INT Priority;		// Priority to run this thread at.
	BOOL Verify;		// Only for loop and read test. If true, verifies data integrity. 
	BOOL VerifyDetails;	// If true, prints detailed information for each invalid byte.
    enum BENCHMARK_DEVICE_TEST_TYPE TestType;	// The benchmark test type.
	enum BENCHMARK_TRANSFER_MODE TransferMode;	// Sync or Async

    // Internal value use during the test.
    //
    usb_dev_handle* DeviceHandle;
	struct usb_device* Device;
    BOOL IsCancelled;
    BOOL IsUserAborted;

	BYTE* VerifyBuffer;		// Stores the verify test pattern for 1 packet.
	WORD VerifyBufferSize;	// Size of VerifyBuffer
};

// The benchmark transfer context used for asynchronous transfers.  see TransferAsync().
struct BENCHMARK_TRANSFER_HANDLE
{
	VOID* Context;
	BOOL InUse;
	CHAR* Data;
	INT DataMaxLength;
	INT ReturnCode;
};

// Holds all of the information about a transfer.
struct BENCHMARK_TRANSFER_PARAM
{
    struct BENCHMARK_TEST_PARAM* Test;

    HANDLE ThreadHandle;
    DWORD ThreadID;
	struct usb_endpoint_descriptor Ep;
	INT IsoPacketSize;
    BOOL IsRunning;

    LONGLONG TotalTransferred;
	LONG LastTransferred;

    LONG Packets;
    DWORD StartTick;
    DWORD LastTick;
    DWORD LastStartTick;

    INT TotalTimeoutCount;
    INT RunningTimeoutCount;
	
	INT TotalErrorCount;
	INT RunningErrorCount;

	INT ShortTransferCount;

	INT TransferHandleNextIndex;
	INT TransferHandleWaitIndex;
	INT OutstandingTransferCount;

	struct BENCHMARK_TRANSFER_HANDLE TransferHandles[MAX_OUTSTANDING_TRANSFERS];

	// Placeholder for end of structure; this is where the raw data for the
	// transfer buffer is allocated.
	//
    BYTE Buffer[0];
};

// Benchmark device api.
struct usb_dev_handle* Bench_Open(WORD vid,	WORD pid, INT interfaceNumber, struct usb_device** deviceForHandle);
int Bench_SetTestType(struct usb_dev_handle* dev, enum BENCHMARK_DEVICE_TEST_TYPE testType, int intf);
int Bench_GetTestType(struct usb_dev_handle* dev, enum BENCHMARK_DEVICE_TEST_TYPE* testType, int intf);

// Critical section for running status. 
CRITICAL_SECTION DisplayCriticalSection;

// Finds the interface for [interface_number] in a libusb-win32 config descriptor.
// If first_interface is not NULL, it is set to the first interface in the config.
//
struct usb_interface_descriptor* usb_find_interface(struct usb_config_descriptor* config_descriptor,
	INT interface_number,
	struct usb_interface_descriptor** first_interface);

// Internal function used by the benchmark application.
void ShowHelp(void);
void ShowCopyright(void);
void SetTestDefaults(struct BENCHMARK_TEST_PARAM* test);
char* GetParamStrValue(const char* src, const char* paramName);
BOOL GetParamIntValue(const char* src, const char* paramName, INT* returnValue);
int ValidateBenchmarkArgs(struct BENCHMARK_TEST_PARAM* testParam);
int ParseBenchmarkArgs(struct BENCHMARK_TEST_PARAM* testParams, int argc, char **argv);
void FreeTransferParam(struct BENCHMARK_TRANSFER_PARAM** testTransferRef);
struct BENCHMARK_TRANSFER_PARAM* CreateTransferParam(struct BENCHMARK_TEST_PARAM* test, int endpointID);
void GetAverageBytesSec(struct BENCHMARK_TRANSFER_PARAM* transferParam, DOUBLE* bps);
void GetCurrentBytesSec(struct BENCHMARK_TRANSFER_PARAM* transferParam, DOUBLE* bps);
void ShowRunningStatus(struct BENCHMARK_TRANSFER_PARAM* transferParam);
void ShowTestInfo(struct BENCHMARK_TEST_PARAM* testParam);
void ShowTransferInfo(struct BENCHMARK_TRANSFER_PARAM* transferParam);

void WaitForTestTransfer(struct BENCHMARK_TRANSFER_PARAM* transferParam);
void ResetRunningStatus(struct BENCHMARK_TRANSFER_PARAM* transferParam);

// The thread transfer routine.
DWORD TransferThreadProc(struct BENCHMARK_TRANSFER_PARAM* transferParams);

#define TRANSFER_DISPLAY(TransferParam, ReadingString, WritingString) \
	((TransferParam->Ep.bEndpointAddress & USB_ENDPOINT_DIR_MASK) ? ReadingString : WritingString)

#define INC_ROLL(IncField, RollOverValue) if ((++IncField) >= RollOverValue) IncField = 0

#define ENDPOINT_TYPE(TransferParam) (TransferParam->Ep.bmAttributes & 3)
const char* TestDisplayString[] = {"None", "Read", "Write", "Loop", NULL};
const char* EndpointTypeDisplayString[] = {"Control", "Isochronous", "Bulk", "Interrupt", NULL};

void SetTestDefaults(struct BENCHMARK_TEST_PARAM* test)
{
    memset(test,0,sizeof(struct BENCHMARK_TEST_PARAM));

    test->Ep			= 0x00;
    test->Vid			= 0x0666;
    test->Pid			= 0x0001;
    test->Refresh		= 1000;
    test->Timeout		= 5000;
    test->TestType		= TestTypeLoop;
    test->BufferSize	= 4096;
    test->BufferCount   = 1;
    test->Priority		= THREAD_PRIORITY_NORMAL;
}

struct usb_interface_descriptor* usb_find_interface(struct usb_config_descriptor* config_descriptor,
	INT interface_number,
	struct usb_interface_descriptor** first_interface)
{
	struct usb_interface_descriptor* intf;
	int intfIndex;

	if (first_interface) 
		*first_interface = NULL;

	if (!config_descriptor) return NULL;

	for (intfIndex = 0; intfIndex < config_descriptor->bNumInterfaces; intfIndex++)
	{
		if (config_descriptor->interface[intfIndex].num_altsetting)
		{
			intf = &config_descriptor->interface[intfIndex].altsetting[0];
			if ((first_interface) && *first_interface == NULL)
				*first_interface = intf;

			if (intf->bInterfaceNumber == interface_number)
				return intf;
		}
	}

	return NULL;
}
struct usb_dev_handle* Bench_Open(WORD vid, WORD pid, INT interfaceNumber, struct usb_device** deviceForHandle)
{
    struct usb_bus* bus;
    struct usb_device* dev;
    struct usb_dev_handle* udev;

    for (bus = usb_get_busses(); bus; bus = bus->next)
    {
        for (dev = bus->devices; dev; dev = dev->next)
        {
            if (dev->descriptor.idVendor == vid && dev->descriptor.idProduct == pid)
            {
				if ((udev = usb_open(dev)))
				{
					if (dev->descriptor.bNumConfigurations)
					{
						if (usb_find_interface(&dev->config[0], interfaceNumber, NULL) != NULL)
						{
							if (deviceForHandle) *deviceForHandle = dev;
							return udev;
						}
					}

					usb_close(udev);
				}
            }
        }
    }
    return NULL;
}

int Bench_SetTestType(struct usb_dev_handle* dev, enum BENCHMARK_DEVICE_TEST_TYPE testType, int intf)
{
    char buffer[1];
    int ret = 0;

    ret = usb_control_msg(dev,
                          USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN,
                          SET_TEST, testType, intf,
                          buffer, 1,
                          1000);
    return ret;
}

int Bench_GetTestType(struct usb_dev_handle* dev, enum BENCHMARK_DEVICE_TEST_TYPE* testType, int intf)
{
    char buffer[1];
    int ret = 0;

    ret = usb_control_msg(dev,
                          USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN,
                          GET_TEST, 0, intf,
                          buffer, 1,
                          1000);

    if (ret == 1)
        *testType = buffer[0];

    return ret;
}
enum TRANSFER_VERIFY_STATE
{
	TVS_START,
	TVS_KEY,
	TVS_DATA,
	TVS_FIND_START,

};

INT VerifyData(struct BENCHMARK_TRANSFER_PARAM* transferParam, BYTE* data, INT dataLength)
{

	WORD verifyDataSize = transferParam->Test->VerifyBufferSize;
	BYTE* verifyData = transferParam->Test->VerifyBuffer;
	BYTE keyC = 0;
	BOOL seedKey = TRUE;
	INT dataLeft = dataLength;
	INT dataIndex = 0;
	INT packetIndex = 0;
	INT verifyIndex = 0;

	while(dataLeft > 1)
	{
		verifyDataSize = dataLeft > transferParam->Test->VerifyBufferSize ? transferParam->Test->VerifyBufferSize : dataLeft;

		if (seedKey)
			keyC = data[dataIndex+1];
		else
		{
			if (data[dataIndex+1]==0)
			{
				keyC=0;
			}
			else
			{
				keyC++;
			}
		}
		seedKey = FALSE;
		// Index 0 is always 0.
		// The key is always at index 1
		verifyData[1] = keyC;
		if (memcmp(&data[dataIndex],verifyData,verifyDataSize) != 0)
		{
			// Packet verification failed.

			// Reset the key byte on the next packet.
			seedKey = TRUE;

			CONVDAT("data mismatch packet-index=%d data-index=%d\n", packetIndex, dataIndex);

			if (transferParam->Test->VerifyDetails)
			{
				for (verifyIndex=0; verifyIndex<verifyDataSize; verifyIndex++)
				{
					if (verifyData[verifyIndex] == data[dataIndex + verifyIndex])
						continue;

					CONVDAT("packet-offset=%d expected %02Xh got %02Xh\n",
						verifyIndex,
						verifyData[verifyIndex],
						data[dataIndex+verifyIndex]);

				}
			}

		}

		// Move to the next packet.
		packetIndex++;
		dataLeft -= verifyDataSize;
		dataIndex+= verifyDataSize;

	}

	return 0;
}

int TransferSync(struct BENCHMARK_TRANSFER_PARAM* transferParam)
{
	int ret;
	if (transferParam->Ep.bEndpointAddress & USB_ENDPOINT_DIR_MASK)
	{
		ret = usb_bulk_read(
				  transferParam->Test->DeviceHandle, transferParam->Ep.bEndpointAddress,
				  transferParam->Buffer, transferParam->Test->BufferSize,
				  transferParam->Test->Timeout);
	}
	else
	{
		ret = usb_bulk_write(
				  transferParam->Test->DeviceHandle, transferParam->Ep.bEndpointAddress,
				  transferParam->Buffer, transferParam->Test->BufferSize,
				  transferParam->Test->Timeout);
	}

	return ret;
}

int TransferAsync(struct BENCHMARK_TRANSFER_PARAM* transferParam, struct BENCHMARK_TRANSFER_HANDLE** handleRef)
{
	int ret;
	struct BENCHMARK_TRANSFER_HANDLE* handle;

	*handleRef = NULL;

	// Submit transfers until the maximum number of outstanding transfer(s) is reached.
	while (transferParam->OutstandingTransferCount < transferParam->Test->BufferCount)
	{
		// Get the next available benchmark transfer handle.
		*handleRef = handle = &transferParam->TransferHandles[transferParam->TransferHandleNextIndex];

		// If a libusb-win32 transfer context hasn't been setup for this benchmark transfer
		// handle, do it now.
		//
		if (!handle->Context)
		{
			// Data buffer(s) are located at the end of the transfer param.
			handle->Data = transferParam->Buffer + (transferParam->TransferHandleNextIndex * transferParam->Test->BufferSize);
			handle->DataMaxLength = transferParam->Test->BufferSize;

			
			switch (ENDPOINT_TYPE(transferParam))
			{
			case USB_ENDPOINT_TYPE_ISOCHRONOUS:
				ret = usb_isochronous_setup_async(transferParam->Test->DeviceHandle, 
					&handle->Context,
					transferParam->Ep.bEndpointAddress,
					transferParam->IsoPacketSize ? transferParam->IsoPacketSize : transferParam->Ep.wMaxPacketSize);
				break;
			case USB_ENDPOINT_TYPE_BULK:
				ret = usb_bulk_setup_async(transferParam->Test->DeviceHandle,
					&handle->Context,
					transferParam->Ep.bEndpointAddress);
				break;
			case USB_ENDPOINT_TYPE_INTERRUPT:
				ret = usb_interrupt_setup_async(transferParam->Test->DeviceHandle,
					&handle->Context,
					transferParam->Ep.bEndpointAddress);
				break;
			default:
				ret = -1;
				break;
			}

			if (ret < 0) 
			{
				CONERR("failed creating transfer context ret=%d\n",ret);
				goto Done;
			}
		}


		// Submit this transfer now.
		handle->ReturnCode = ret = usb_submit_async(handle->Context, handle->Data, handle->DataMaxLength);
		if (ret < 0) goto Done;

		// Mark this handle has InUse.
		handle->InUse = TRUE;

		// When transfers ir successfully submitted, OutstandingTransferCount goes up; when
		// they are completed it goes down.
		//
		transferParam->OutstandingTransferCount++;

		// Move TransferHandleNextIndex to the next available transfer.
		INC_ROLL(transferParam->TransferHandleNextIndex, transferParam->Test->BufferCount);

	}

	// If the number of outstanding transfers has reached the limit, wait for the 
	// oldest outstanding transfer to complete.
	//
	if (transferParam->OutstandingTransferCount == transferParam->Test->BufferCount)
	{
		// TransferHandleWaitIndex is the index of the oldest outstanding transfer.
		*handleRef = handle = &transferParam->TransferHandles[transferParam->TransferHandleWaitIndex];

		// Only wait, cancelling & freeing is handled by the caller.
		handle->ReturnCode = ret = usb_reap_async_nocancel(handle->Context, transferParam->Test->Timeout);

		if (ret < 0) goto Done;

		// Mark this handle has no longer InUse.
		handle->InUse = FALSE;

		// When transfers ir successfully submitted, OutstandingTransferCount goes up; when
		// they are completed it goes down.
		//
		transferParam->OutstandingTransferCount--;

		// Move TransferHandleWaitIndex to the oldest outstanding transfer.
		INC_ROLL(transferParam->TransferHandleWaitIndex, transferParam->Test->BufferCount);
	}

Done:
	return ret;
}

DWORD TransferThreadProc(struct BENCHMARK_TRANSFER_PARAM* transferParam)
{
    int ret, i;
	struct BENCHMARK_TRANSFER_HANDLE* handle;
	char* data;
    transferParam->IsRunning = TRUE;

    while (!transferParam->Test->IsCancelled)
    {
		data = NULL;
		handle = NULL;

		if (transferParam->Test->TransferMode == TRANSFER_MODE_SYNC)
		{
			ret = TransferSync(transferParam);
			if (ret >= 0) data = transferParam->Buffer;
		}
		else if (transferParam->Test->TransferMode == TRANSFER_MODE_ASYNC)
		{
			ret = TransferAsync(transferParam, &handle);
			if ((handle) && ret >= 0) data = handle->Data;
		}
		else
		{
            CONERR("invalid transfer mode %d\n",transferParam->Test->TransferMode);
			goto Done;
		}
        if (ret < 0)
        {
			// The user pressed 'Q'.
            if (transferParam->Test->IsUserAborted) break;

			// Transfer timed out
            if (ret == TRANSFER_TIMEDOUT)
            {
                transferParam->TotalTimeoutCount++;
                transferParam->RunningTimeoutCount++;
                CONWRN("Timeout #%d %s on Ep%02Xh..\n",
                       transferParam->RunningTimeoutCount,
                       TRANSFER_DISPLAY(transferParam,"reading","writing"),
                       transferParam->Ep.bEndpointAddress);

                if (transferParam->RunningTimeoutCount > transferParam->Test->Retry)
                    break;
            }
            else
            {
				// An error (other than a timeout) occured.

				// usb_strerror()is not thread safe and should not be used
				// in a multi-threaded app.  It's used here because
				// this is a test program.
				//

				transferParam->TotalErrorCount++;
                transferParam->RunningErrorCount++;
				CONERR("failed %s! %d of %d ret=%d: %s\n",
					TRANSFER_DISPLAY(transferParam,"reading","writing"),
					transferParam->RunningErrorCount,
					transferParam->Test->Retry+1,
					ret,
					usb_strerror());

				usb_resetep(transferParam->Test->DeviceHandle, transferParam->Ep.bEndpointAddress);

                if (transferParam->RunningErrorCount > transferParam->Test->Retry)
                    break;

            }
			ret = 0;
        }
        else
        {
			if (ret < transferParam->Test->BufferSize && !transferParam->Test->IsCancelled)
			{
				if (ret > 0)
				{
					transferParam->ShortTransferCount++;
					CONWRN("Short transfer on Ep%02Xh expected %d got %d.\n",
						transferParam->Ep.bEndpointAddress,
						transferParam->Test->BufferSize,
						ret);
				}
				else
				{
					CONWRN("Zero-length transfer on Ep%02Xh expected %d.\n",
						transferParam->Ep.bEndpointAddress,
						transferParam->Test->BufferSize);

					transferParam->TotalErrorCount++;
					transferParam->RunningErrorCount++;
					if (transferParam->RunningErrorCount > transferParam->Test->Retry)
						break;
					usb_resetep(transferParam->Test->DeviceHandle, transferParam->Ep.bEndpointAddress);
				}
			}
			else
			{
				transferParam->RunningErrorCount = 0;
				transferParam->RunningTimeoutCount = 0;
			}

			if ((transferParam->Test->Verify) && 
				(transferParam->Ep.bEndpointAddress & USB_ENDPOINT_DIR_MASK))
			{
				VerifyData(transferParam, data, ret);
			}
        }

        EnterCriticalSection(&DisplayCriticalSection);

        if (!transferParam->StartTick && transferParam->Packets >= 0)
        {
            transferParam->StartTick = GetTickCount();
			transferParam->LastStartTick	= transferParam->StartTick;
            transferParam->LastTick			= transferParam->StartTick;

			transferParam->LastTransferred = 0;
            transferParam->TotalTransferred = 0;
            transferParam->Packets = 0;
        }
        else
        {
			if (!transferParam->LastStartTick)
			{
				transferParam->LastStartTick	= transferParam->LastTick;
				transferParam->LastTransferred = 0;
			}
            transferParam->LastTick			= GetTickCount();
 
			transferParam->LastTransferred  += ret;
            transferParam->TotalTransferred += ret;
            transferParam->Packets++;
        }

        LeaveCriticalSection(&DisplayCriticalSection);
    }

Done:

	for (i=0; i < transferParam->Test->BufferCount; i++)
	{
		if (transferParam->TransferHandles[i].Context)
		{
			if (transferParam->TransferHandles[i].InUse)
			{
				if ((ret = usb_cancel_async(transferParam->TransferHandles[i].Context)) < 0)
					if (!transferParam->Test->IsUserAborted)
						CONERR("failed cancelling transfer! ret=%d\n",ret);

				transferParam->TransferHandles[i].InUse=FALSE;
			}
			usb_free_async(&transferParam->TransferHandles[i].Context);
		}
	}

    transferParam->IsRunning = FALSE;
    return 0;
}

char* GetParamStrValue(const char* src, const char* paramName)
{
	return (strstr(src,paramName)==src) ? (char*)(src+strlen(paramName)) : NULL;
}

BOOL GetParamIntValue(const char* src, const char* paramName, INT* returnValue)
{
    char* value = GetParamStrValue(src, paramName);
    if (value)
    {
        *returnValue = strtol(value, NULL, 0);
        return TRUE;
    }
    return FALSE;
}

int ValidateBenchmarkArgs(struct BENCHMARK_TEST_PARAM* testParam)
{
    if (testParam->BufferCount < 1 || testParam->BufferCount > MAX_OUTSTANDING_TRANSFERS)
    {
		CONERR("Invalid BufferCount argument %d. BufferCount must be greater than 0 and less than or equal to %d.\n",
			testParam->BufferCount, MAX_OUTSTANDING_TRANSFERS);
        return -1;
    }

    return 0;
}

int ParseBenchmarkArgs(struct BENCHMARK_TEST_PARAM* testParams, int argc, char **argv)
{
#define GET_INT_VAL
    char arg[128];
    char* value;
    int iarg;

    for (iarg=1; iarg < argc; iarg++)
    {
		if (strcpy_s(arg, _countof(arg), argv[iarg])!=ERROR_SUCCESS)
			return -1;

        strlwr(arg);

        if      (GetParamIntValue(arg, "vid=", &testParams->Vid)) {}
        else if (GetParamIntValue(arg, "pid=", &testParams->Pid)) {}
        else if (GetParamIntValue(arg, "retry=", &testParams->Retry)) {}
        else if (GetParamIntValue(arg, "buffercount=", &testParams->BufferCount)) 
		{
			if (testParams->BufferCount > 1)
				testParams->TransferMode = TRANSFER_MODE_ASYNC;
		}
        else if (GetParamIntValue(arg, "buffersize=", &testParams->BufferSize)) {}
        else if (GetParamIntValue(arg, "size=", &testParams->BufferSize)) {}
        else if (GetParamIntValue(arg, "timeout=", &testParams->Timeout)) {}
        else if (GetParamIntValue(arg, "intf=", &testParams->Intf)) {}
        else if (GetParamIntValue(arg, "ep=", &testParams->Ep)) 
		{
			testParams->Ep &= 0xf;
		}
        else if (GetParamIntValue(arg, "refresh=", &testParams->Refresh)) {}
        else if (GetParamIntValue(arg, "isopacketsize=", &testParams->IsoPacketSize)) {}
        else if ((value=GetParamStrValue(arg,"mode=")))
        {
            if (GetParamStrValue(value,"sync"))
            {
				testParams->TransferMode = TRANSFER_MODE_SYNC;
            }
            else if (GetParamStrValue(value,"async"))
            {
				testParams->TransferMode = TRANSFER_MODE_ASYNC;
            }
             else
            {
                // Invalid EndpointType argument.
                CONERR("invalid transfer mode argument! %s\n",argv[iarg]);
                return -1;

            }
        }
        else if ((value=GetParamStrValue(arg,"priority=")))
        {
            if (GetParamStrValue(value,"lowest"))
            {
                testParams->Priority=THREAD_PRIORITY_LOWEST;
            }
            else if (GetParamStrValue(value,"belownormal"))
            {
                testParams->Priority=THREAD_PRIORITY_BELOW_NORMAL;
            }
            else if (GetParamStrValue(value,"normal"))
            {
                testParams->Priority=THREAD_PRIORITY_NORMAL;
            }
            else if (GetParamStrValue(value,"abovenormal"))
            {
                testParams->Priority=THREAD_PRIORITY_ABOVE_NORMAL;
            }
            else if (GetParamStrValue(value,"highest"))
            {
                testParams->Priority=THREAD_PRIORITY_HIGHEST;
            }
            else
            {
                CONERR("invalid priority argument! %s\n",argv[iarg]);
                return -1;
            }
        }
        else if (!stricmp(arg,"notestselect"))
        {
            testParams->NoTestSelect = TRUE;
        }
        else if (!stricmp(arg,"read"))
        {
            testParams->TestType = TestTypeRead;
        }
        else if (!stricmp(arg,"write"))
        {
            testParams->TestType = TestTypeWrite;
        }
        else if (!stricmp(arg,"loop"))
        {
            testParams->TestType = TestTypeLoop;
        }
        else if (!stricmp(arg,"list"))
        {
            testParams->UseList = TRUE;
        }
        else if (!stricmp(arg,"verifydetails"))
        {
            testParams->VerifyDetails = TRUE;
            testParams->Verify = TRUE;
        }
        else if (!stricmp(arg,"verify"))
        {
            testParams->Verify = TRUE;
        }
		else
        {
            CONERR("invalid argument! %s\n",argv[iarg]);
            return -1;
        }
    }
    return ValidateBenchmarkArgs(testParams);
}

INT CreateVerifyBuffer(struct BENCHMARK_TEST_PARAM* testParam, WORD endpointMaxPacketSize)
{
	int i;
	BYTE indexC = 0;
	testParam->VerifyBuffer = malloc(endpointMaxPacketSize);
	if (!testParam->VerifyBuffer)
	{
        CONERR("memory allocation failure at line %d!\n",__LINE__);
        return -1;
	}

	testParam->VerifyBufferSize = endpointMaxPacketSize;

	for(i=0; i < endpointMaxPacketSize; i++)
	{
	   testParam->VerifyBuffer[i] = indexC++;
	   if (indexC == 0) indexC = 1;
	}

	return 0;
}

void FreeTransferParam(struct BENCHMARK_TRANSFER_PARAM** testTransferRef)
{
	struct BENCHMARK_TRANSFER_PARAM* pTransferParam;

	if ((!testTransferRef) || !*testTransferRef) return;
	pTransferParam = *testTransferRef;

    if (pTransferParam->ThreadHandle)
    {
        CloseHandle(pTransferParam->ThreadHandle);
        pTransferParam->ThreadHandle = NULL;
    }

    free(pTransferParam);

    *testTransferRef = NULL;
}

struct BENCHMARK_TRANSFER_PARAM* CreateTransferParam(struct BENCHMARK_TEST_PARAM* test, int endpointID)
{
    struct BENCHMARK_TRANSFER_PARAM* transferParam;
	struct usb_interface_descriptor* testInterface;
	int i;
    int allocSize = sizeof(struct BENCHMARK_TRANSFER_PARAM)+(test->BufferSize * test->BufferCount);

    transferParam = (struct BENCHMARK_TRANSFER_PARAM*) malloc(allocSize);

    if (transferParam)
    {
        memset(transferParam, 0, allocSize);
        transferParam->Test = test;
		if (!(testInterface = usb_find_interface(&test->Device->config[0], test->Intf, NULL)))
		{
            CONERR("failed locating interface %02Xh!\n", test->Intf);
            FreeTransferParam(&transferParam);
			goto Done;
		}

		for(i=0; i < testInterface->bNumEndpoints; i++)
		{
			if (!(endpointID & USB_ENDPOINT_ADDRESS_MASK))
			{
				// Use first endpoint that matches the direction
				if ((testInterface->endpoint[i].bEndpointAddress & USB_ENDPOINT_DIR_MASK) == endpointID)
				{
					memcpy(&transferParam->Ep, &testInterface->endpoint[i],sizeof(struct usb_endpoint_descriptor));
					break;
				}
			}
			else
			{
				if ((int)testInterface->endpoint[i].bEndpointAddress == endpointID)
				{
					memcpy(&transferParam->Ep, &testInterface->endpoint[i],sizeof(struct usb_endpoint_descriptor));
					break;
				}
			}
		}
        if (!transferParam->Ep.bEndpointAddress)
        {
            CONERR("failed locating EP%02Xh!\n", endpointID);
            FreeTransferParam(&transferParam);
			goto Done;
        }

        if (transferParam->Test->BufferSize % transferParam->Ep.wMaxPacketSize)
        {
            CONERR("buffer size %d is not an interval of EP%02Xh maximum packet size of %d!\n",
				transferParam->Test->BufferSize,
				transferParam->Ep.bEndpointAddress,
				transferParam->Ep.wMaxPacketSize);

            FreeTransferParam(&transferParam);
			goto Done;
        }

		if (test->IsoPacketSize)
			transferParam->IsoPacketSize = test->IsoPacketSize;
		else
			transferParam->IsoPacketSize = transferParam->Ep.wMaxPacketSize;

		if (ENDPOINT_TYPE(transferParam) == USB_ENDPOINT_TYPE_ISOCHRONOUS)
			transferParam->Test->TransferMode = TRANSFER_MODE_ASYNC;

        ResetRunningStatus(transferParam);

        transferParam->ThreadHandle = CreateThread(
                                          NULL,
                                          0,
                                          (LPTHREAD_START_ROUTINE)TransferThreadProc,
                                          transferParam,
                                          CREATE_SUSPENDED,
                                          &transferParam->ThreadID);

        if (!transferParam->ThreadHandle)
        {
            CONERR0("failed creating thread!\n");
            FreeTransferParam(&transferParam);
			goto Done;
        }

		// If verify mode is on, this is a loop test, and this is a write endpoint, fill
		// the buffers with the same test data sent by a benchmark device when running
		// a read only test.
		if (transferParam->Test->Verify &&
			transferParam->Test->TestType == TestTypeLoop &&
			!(transferParam->Ep.bEndpointAddress & USB_ENDPOINT_DIR_MASK))
		{
			// Data Format:
			// [0][KeyByte] 2 3 4 5 ..to.. wMaxPacketSize (if data byte rolls it is incremented to 1)
			// Increment KeyByte and repeat
			//
			BYTE indexC=0;
			INT bufferIndex = 0;
			WORD dataIndex;
			INT packetIndex;
			INT packetCount = ((transferParam->Test->BufferCount*transferParam->Test->BufferSize) / transferParam->Ep.wMaxPacketSize);
			for(packetIndex = 0; packetIndex < packetCount; packetIndex++)
			{
				indexC = 2;
				for (dataIndex=0; dataIndex < transferParam->Ep.wMaxPacketSize; dataIndex++)
				{
					if (dataIndex == 0)			// Start
						transferParam->Buffer[bufferIndex] = 0;
					else if (dataIndex == 1)	// Key
						transferParam->Buffer[bufferIndex] = packetIndex & 0xFF;
					else						// Data
						transferParam->Buffer[bufferIndex] = indexC++;

					// if wMaxPacketSize is > 255, indexC resets to 1.
					if (indexC == 0) indexC = 1;

					bufferIndex++;
				}
			}
		}
    }

Done:
    if (!transferParam)
        CONERR0("failed creating transfer param!\n");

    return transferParam;
}

void GetAverageBytesSec(struct BENCHMARK_TRANSFER_PARAM* transferParam, DOUBLE* bps)
{
	DOUBLE ticksSec;
    if ((!transferParam->StartTick) || 
		(transferParam->StartTick >= transferParam->LastTick) || 
		transferParam->TotalTransferred==0)
    {
        *bps=0;
    }
    else
    {
		ticksSec = (transferParam->LastTick - transferParam->StartTick) / 1000.0;
		*bps = (transferParam->TotalTransferred / ticksSec);
    }
}

void GetCurrentBytesSec(struct BENCHMARK_TRANSFER_PARAM* transferParam, DOUBLE* bps)
{
	DOUBLE ticksSec;
    if ((!transferParam->StartTick) || 
		(!transferParam->LastStartTick) || 
		(transferParam->LastTick <= transferParam->LastStartTick) || 
		transferParam->LastTransferred==0)
    {
        *bps=0;
    }
    else
    {
		ticksSec = (transferParam->LastTick - transferParam->LastStartTick) / 1000.0;
		*bps = transferParam->LastTransferred / ticksSec;
    }
}

void ShowRunningStatus(struct BENCHMARK_TRANSFER_PARAM* transferParam)
{
	struct BENCHMARK_TRANSFER_PARAM temp;
	DOUBLE bpsOverall;
	DOUBLE bpsLastTransfer;
    
	// LOCK the display critical section
    EnterCriticalSection(&DisplayCriticalSection);
	
	memcpy(&temp, transferParam, sizeof(struct BENCHMARK_TRANSFER_PARAM));
	
	// UNLOCK the display critical section
    LeaveCriticalSection(&DisplayCriticalSection);

    if ((!temp.StartTick) || (temp.StartTick >= temp.LastTick))
    {
        CONMSG("Synchronizing %d..\n", abs(transferParam->Packets));
    }
    else
    {
        GetAverageBytesSec(&temp,&bpsOverall);
        GetCurrentBytesSec(&temp,&bpsLastTransfer);
		transferParam->LastStartTick = 0;
		CONMSG("Avg. Bytes/s: %.2f Transfers: %d Bytes/s: %.2f\n",
			bpsOverall, temp.Packets, bpsLastTransfer);
    }

}
void ShowTransferInfo(struct BENCHMARK_TRANSFER_PARAM* transferParam)
{
    DOUBLE bpsAverage;
    DOUBLE bpsCurrent;
    DOUBLE elapsedSeconds;

	if (!transferParam) return;

	CONMSG("%s %s (Ep%02Xh) max packet size: %d\n",
		EndpointTypeDisplayString[ENDPOINT_TYPE(transferParam)],
		TRANSFER_DISPLAY(transferParam,"Read","Write"),
		transferParam->Ep.bEndpointAddress,
		transferParam->Ep.wMaxPacketSize);

	if (transferParam->StartTick)
    {
        GetAverageBytesSec(transferParam,&bpsAverage);
        GetCurrentBytesSec(transferParam,&bpsCurrent);
        CONMSG("\tTotal Bytes     : %I64d\n", transferParam->TotalTransferred);
        CONMSG("\tTotal Transfers : %d\n", transferParam->Packets);

		if (transferParam->ShortTransferCount)
		{
			CONMSG("\tShort Transfers : %d\n", transferParam->ShortTransferCount);
		}
		if (transferParam->TotalTimeoutCount)
		{
			CONMSG("\tTimeout Errors  : %d\n", transferParam->TotalTimeoutCount);
		}
		if (transferParam->TotalErrorCount)
		{
			CONMSG("\tOther Errors    : %d\n", transferParam->TotalErrorCount);
		}

        CONMSG("\tAvg. Bytes/sec  : %.2f\n", bpsAverage);

		if (transferParam->StartTick && transferParam->StartTick < transferParam->LastTick)
		{
			elapsedSeconds = (transferParam->LastTick - transferParam->StartTick) / 1000.0;

			CONMSG("\tElapsed Time    : %.2f seconds\n", elapsedSeconds);
		}

	    CONMSG0("\n");
    }

}

void ShowTestInfo(struct BENCHMARK_TEST_PARAM* testParam)
{
    if (!testParam) return;

    CONMSG("%s Test Information\n",TestDisplayString[testParam->TestType & 3]);
    CONMSG("\tVid / Pid       : %04Xh / %04Xh\n", testParam->Vid,  testParam->Pid);
    CONMSG("\tInterface #     : %02Xh\n", testParam->Intf);
    CONMSG("\tPriority        : %d\n", testParam->Priority);
    CONMSG("\tBuffer Size     : %d\n", testParam->BufferSize);
    CONMSG("\tBuffer Count    : %d\n", testParam->BufferCount);
    CONMSG("\tDisplay Refresh : %d (ms)\n", testParam->Refresh);
    CONMSG("\tTransfer Timeout: %d (ms)\n", testParam->Timeout);
    CONMSG("\tRetry Count     : %d\n", testParam->Retry);
    CONMSG("\tVerify Data     : %s%s\n",
		testParam->Verify ? "On" : "Off",
		(testParam->Verify && testParam->VerifyDetails) ? " (Detailed)" : "");

    CONMSG0("\n");
}

void WaitForTestTransfer(struct BENCHMARK_TRANSFER_PARAM* transferParam)
{
    DWORD exitCode;
    while (transferParam)
    {
        if (!transferParam->IsRunning)
        {
            if (GetExitCodeThread(transferParam->ThreadHandle, &exitCode))
            {
                if (exitCode == 0)
                {
                    CONMSG("stopped Ep%02Xh thread.\tExitCode=%d\n",
                              transferParam->Ep.bEndpointAddress, exitCode);
                    break;
                }
            }
            else
            {
                CONERR("failed getting Ep%02Xh thread exit code!\n",transferParam->Ep.bEndpointAddress);
                break;
            }
        }
        Sleep(100);
        CONMSG("waiting for Ep%02Xh thread..\n", transferParam->Ep.bEndpointAddress);
    }
}
void ResetRunningStatus(struct BENCHMARK_TRANSFER_PARAM* transferParam)
{
    if (!transferParam) return;

    transferParam->StartTick=0;
    transferParam->TotalTransferred=0;
    transferParam->Packets=-2;
    transferParam->LastTick=0;
    transferParam->RunningTimeoutCount=0;
}

int GetTestDeviceFromList(struct BENCHMARK_TEST_PARAM* testParam)
{
    const int LINE_MAX_SIZE   = 1024;
    const int STRING_MAX_SIZE = 256;
    const int NUM_STRINGS = 3;
    const int ALLOC_SIZE = LINE_MAX_SIZE + (STRING_MAX_SIZE * NUM_STRINGS);

    int userInput;

    char* buffer;
    char* line;
    char* product;
    char* manufacturer;
    char* serial;

    struct usb_bus* bus;
    struct usb_device* dev;
    usb_dev_handle* udev;
    struct usb_device* validDevices[256];

    int deviceIndex=0;
	struct usb_interface_descriptor* firstInterface;

    int ret = -1;

    buffer = malloc(ALLOC_SIZE);
    if (!buffer)
    {
        CONERR0("failed allocating memory!\n");
        return ret;
    }

    line = buffer;
    product = buffer + LINE_MAX_SIZE;
    manufacturer = product + STRING_MAX_SIZE;
    serial = manufacturer + STRING_MAX_SIZE;

    for (bus = usb_get_busses(); bus; bus = bus->next)
    {
        for (dev = bus->devices; dev; dev = dev->next)
        {

            udev = usb_open(dev);
            if (udev)
            {
                memset(buffer, 0, ALLOC_SIZE);
                line = buffer;
                if (dev->descriptor.iManufacturer)
                {
                    if (usb_get_string_simple(udev, dev->descriptor.iManufacturer, manufacturer, STRING_MAX_SIZE - 1) > 0)
                    {
                        strcat(line,"(");
                        strcat(line,manufacturer);
                        strcat(line,") ");
                    }
                }

                if (dev->descriptor.iProduct)
                {
                    if (usb_get_string_simple(udev, dev->descriptor.iProduct, product, STRING_MAX_SIZE - 1) > 0)
                    {
                        strcat(line,product);
                        strcat(line," ");
                    }
                }

                if (dev->descriptor.iSerialNumber)
                {
                    if (usb_get_string_simple(udev, dev->descriptor.iSerialNumber, serial, STRING_MAX_SIZE - 1) > 0)
                    {
                        strcat(line,"[");
                        strcat(line,serial);
                        strcat(line,"] ");
                    }
                }

                if (!deviceIndex)
                    CONMSG0("\n");

                validDevices[deviceIndex++] = dev;

                CONMSG("%d. %04X:%04X %s\n",
                          deviceIndex, dev->descriptor.idVendor, dev->descriptor.idProduct, line);

                usb_close(udev);
            }
        }
    }
    
	if (!deviceIndex) 
	{
		CONERR0("No devices where found!\n");
		ret = -1;
		goto Done;
	}

    CONMSG("\nSelect device (1-%d) :",deviceIndex);
	ret = _cscanf("%i",&userInput);
    if (ret != 1 || userInput < 1)
	{
        CONMSG0("\n");
        CONMSG0("Aborting..\n");
		ret = -1;
		goto Done;
	}
    CONMSG0("\n");
    userInput--;
    if (userInput >= 0 && userInput < deviceIndex)
    {
        testParam->DeviceHandle = usb_open(validDevices[userInput]);
        if (testParam->DeviceHandle)
        {
			testParam->Device = validDevices[userInput];
            testParam->Vid = testParam->Device->descriptor.idVendor;
            testParam->Pid = testParam->Device->descriptor.idProduct;
			if (usb_find_interface(&validDevices[userInput]->config[0],testParam->Intf,&firstInterface) == NULL)
			{
				// the specified (or default) interface didn't exist, use the first one.
				if (firstInterface != NULL)
				{
					testParam->Intf = firstInterface->bInterfaceNumber;
				}
				else
				{
					CONERR("device %04X:%04X does not have any interfaces!\n",
						testParam->Vid, testParam->Pid);
					ret = -1;
					goto Done;
				}
			}
            ret = 0;
        }
	}

Done:
	if (buffer)
        free(buffer);

    return ret;
}

int main(int argc, char** argv)
{
    struct BENCHMARK_TEST_PARAM Test;
    struct BENCHMARK_TRANSFER_PARAM* ReadTest	= NULL;
    struct BENCHMARK_TRANSFER_PARAM* WriteTest	= NULL;
    int key;


    if (argc == 1)
    {
        ShowHelp();
        return -1;
    }

	ShowCopyright();

    // NOTE: This is the log level for the benchmark application.
    //
#if defined __ERROR_H__
    usb_log_set_level(255);
#endif

    SetTestDefaults(&Test);

    // Load the command line arguments.
    if (ParseBenchmarkArgs(&Test, argc, argv) < 0)
        return -1;

    // Initialize the critical section used for locking
    // the volatile members of the transfer params in order
    // to update/modify the running statistics.
    //
    InitializeCriticalSection(&DisplayCriticalSection);

    // Initialize the library.
    usb_init();

    // Find all busses.
    usb_find_busses();

    // Find all connected devices.
    usb_find_devices();

    if (Test.UseList)
    {
        if (GetTestDeviceFromList(&Test) < 0)
            goto Done;
    }
    else
    {
        // Open a benchmark device. see Bench_Open().
        Test.DeviceHandle = Bench_Open(Test.Vid, Test.Pid, Test.Intf, &Test.Device);
    }
    if (!Test.DeviceHandle || !Test.Device)
    {
        CONERR("device %04X:%04X not found!\n",Test.Vid, Test.Pid);
        goto Done;
    }
    // If "NoTestSelect" appears in the command line then don't send the control
    // messages for selecting the test type.
    //
    if (!Test.NoTestSelect)
    {
        if (Bench_SetTestType(Test.DeviceHandle, Test.TestType, Test.Intf) != 1)
        {
            CONERR("setting bechmark test type #%d!\n%s\n", Test.TestType, usb_strerror());
            goto Done;
        }
    }

    CONMSG("Benchmark device %04X:%04X opened..\n",Test.Vid, Test.Pid);

    // If reading from the device create the read transfer param. This will also create
    // a thread in a suspended state.
    //
    if (Test.TestType & TestTypeRead)
    {
        ReadTest = CreateTransferParam(&Test, Test.Ep | USB_ENDPOINT_DIR_MASK);
        if (!ReadTest) goto Done;
    }

    // If writing to the device create the write transfer param. This will also create
    // a thread in a suspended state.
    //
    if (Test.TestType & TestTypeWrite)
    {
        WriteTest = CreateTransferParam(&Test, Test.Ep);
        if (!WriteTest) goto Done;
    }

    // Set configuration #1.
    if (usb_set_configuration(Test.DeviceHandle, 1) < 0)
    {
        CONERR("setting configuration #%d!\n%s\n",1,usb_strerror());
        goto Done;
    }

    // Claim_interface Test.Intf (Default is #0)
    if (usb_claim_interface(Test.DeviceHandle, Test.Intf) < 0)
    {
        CONERR("claiming interface #%d!\n%s\n", Test.Intf, usb_strerror());
        goto Done;
    }

	if (Test.Verify)
	{
		if (ReadTest && WriteTest)
		{
			if (CreateVerifyBuffer(&Test, WriteTest->Ep.wMaxPacketSize) < 0)
				goto Done;
		}
		else if (ReadTest)
		{
			if (CreateVerifyBuffer(&Test, ReadTest->Ep.wMaxPacketSize) < 0)
				goto Done;
		}
	}

	ShowTestInfo(&Test);
	ShowTransferInfo(ReadTest);
	ShowTransferInfo(WriteTest);

	CONMSG0("\nWhile the test is running:\n");
	CONMSG0("Press 'Q' to quit\n");
	CONMSG0("Press 'T' for test details\n");
	CONMSG0("Press 'I' for status information\n");
	CONMSG0("Press 'R' to reset averages\n");
    CONMSG0("\nPress 'Q' to exit, any other key to begin..");
    key = _getch();
    CONMSG0("\n");

    if (key=='Q' || key=='q') goto Done;

    // Set the thread priority and start it.
    if (ReadTest)
    {
        SetThreadPriority(ReadTest->ThreadHandle, Test.Priority);
        ResumeThread(ReadTest->ThreadHandle);
    }

    // Set the thread priority and start it.
    if (WriteTest)
    {
        SetThreadPriority(WriteTest->ThreadHandle, Test.Priority);
        ResumeThread(WriteTest->ThreadHandle);
    }

    while (!Test.IsCancelled)
    {
        Sleep(Test.Refresh);

        if (_kbhit())
        {
            // A key was pressed.
            key = _getch();
            switch (key)
            {
            case 'Q':
            case 'q':
                Test.IsUserAborted = TRUE;
                Test.IsCancelled = TRUE;
                break;
			case 'T':
			case 't':
				ShowTestInfo(&Test);
				break;
            case 'I':
            case 'i':
                // LOCK the display critical section
                EnterCriticalSection(&DisplayCriticalSection);

                // Print benchmark test details.
				ShowTransferInfo(ReadTest);
				ShowTransferInfo(WriteTest);


                // UNLOCK the display critical section
                LeaveCriticalSection(&DisplayCriticalSection);
                break;

            case 'R':
            case 'r':
                // LOCK the display critical section
                EnterCriticalSection(&DisplayCriticalSection);

                // Reset the running status.
                ResetRunningStatus(ReadTest);
                ResetRunningStatus(WriteTest);

                // UNLOCK the display critical section
                LeaveCriticalSection(&DisplayCriticalSection);
                break;
            }

            // Only one key at a time.
            while (_kbhit()) _getch();
        }

        // If the read test should be running and it isn't, cancel the test.
        if ((ReadTest) && !ReadTest->IsRunning)
        {
            Test.IsCancelled = TRUE;
            break;
        }

        // If the write test should be running and it isn't, cancel the test.
        if ((WriteTest) && !WriteTest->IsRunning)
        {
            Test.IsCancelled = TRUE;
            break;
        }

        // Print benchmark stats
        if (ReadTest)
            ShowRunningStatus(ReadTest);
        else
            ShowRunningStatus(WriteTest);

    }

	// Wait for the transfer threads to complete gracefully if it
	// can be done in 10ms. All of the code from this point to
	// WaitForTestTransfer() is not required.  It is here only to
	// improve response time when the test is cancelled.
	//
    Sleep(10);

	// If the thread is still running, abort and reset the endpoint.
    if ((ReadTest) && ReadTest->IsRunning)
        usb_resetep(Test.DeviceHandle, ReadTest->Ep.bEndpointAddress);

    // If the thread is still running, abort and reset the endpoint.
    if ((WriteTest) && WriteTest->IsRunning)
        usb_resetep(Test.DeviceHandle, WriteTest->Ep.bEndpointAddress);

    // Small delay incase usb_resetep() was called.
    Sleep(10);

    // WaitForTestTransfer will not return until the thread
	// has exited.
    WaitForTestTransfer(ReadTest);
    WaitForTestTransfer(WriteTest);

    // Print benchmark detailed stats
	ShowTestInfo(&Test);
	if (ReadTest) ShowTransferInfo(ReadTest);
	if (WriteTest) ShowTransferInfo(WriteTest);


Done:
    if (Test.DeviceHandle)
    {
        usb_close(Test.DeviceHandle);
        Test.DeviceHandle = NULL;
    }
	if (Test.VerifyBuffer)
	{
		free(Test.VerifyBuffer);
		Test.VerifyBuffer = NULL;

	}
    FreeTransferParam(&ReadTest);
    FreeTransferParam(&WriteTest);

    DeleteCriticalSection(&DisplayCriticalSection);

    CONMSG0("Press any key to exit..");
    _getch();
    CONMSG0("\n");

    return 0;
}

//////////////////////////////////////////////////////////////////////////////
/* END OF PROGRAM                                                           */
//////////////////////////////////////////////////////////////////////////////
void ShowHelp(void)
{
	#define ID_HELP_TEXT  10020
	#define ID_DOS_TEXT   300

	CONST CHAR* src;
	DWORD src_count, charsWritten;
	HGLOBAL res_data;
	HANDLE handle;
	HRSRC hSrc;

	ShowCopyright();

	hSrc = FindResourceA(NULL, MAKEINTRESOURCEA(ID_HELP_TEXT),MAKEINTRESOURCEA(ID_DOS_TEXT));
	if (!hSrc)	return;

	src_count = SizeofResource(NULL, hSrc);

	res_data = LoadResource(NULL, hSrc);
	if (!res_data)	return;

	src = (char*) LockResource(res_data);
	if (!src) return;

	if ((handle=GetStdHandle(STD_ERROR_HANDLE)) != INVALID_HANDLE_VALUE)
		WriteConsoleA(handle, src, src_count, &charsWritten, NULL);
}

void ShowCopyright(void)
{
	CONMSG0("libusb-win32 USB Benchmark v" RC_VERSION_STR "\n");
	CONMSG0("Copyright (c) 2010 Travis Robinson. <libusbdotnet@gmail.com>\n");
	CONMSG0("website: http://sourceforge.net/projects/libusbdotnet\n");
}