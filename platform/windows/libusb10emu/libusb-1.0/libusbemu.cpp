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

#include "libusb.h"
#include <usb.h>
#include <cassert>
#include <vector>
#include <map>
#include <algorithm>
#include <windows.h>
#include <conio.h>
#include "freenect_internal.h"

// Template Machinery to safely cast pointers from one library type to another
template<typename T> struct LibTranslator { typedef void TranslatedType; };
// libusb_device <-> usb_device_t
template<> struct LibTranslator<libusb_device>  { typedef struct usb_device TranslatedType; };
template<> struct LibTranslator<struct usb_device>   { typedef libusb_device TranslatedType; };
// libusb_device_handle <-> usb_dev_handle
template<> struct LibTranslator<libusb_device_handle> { typedef usb_dev_handle       TranslatedType; };
template<> struct LibTranslator<usb_dev_handle>       { typedef libusb_device_handle TranslatedType; };
// translator() helper function
template<typename T>
typename LibTranslator<T>::TranslatedType* translate(const T* p)
{
	return((LibTranslator<T>::TranslatedType*)p);
}

int libusb_init(libusb_context** context)
{
	usb_init();
	// there is no such a thing like 'context' in libusb-0.1...
	// however, it is a good idea to return something just in the case that
	// the callee decides to test if the returned context is NULL instead of
	// examining the function return value...
	*context = (libusb_context*) new void* (NULL);
	// 0 on success; LIBUSB_ERROR on failure
	return(0);
}

void libusb_exit(libusb_context* ctx)
{
	delete((void*)ctx);
}

ssize_t libusb_get_device_list(libusb_context* ctx, libusb_device*** list)
{
	// libusb_device*** list demystified:
	// libusb_device*** is the C equivalent to libusb_device**& in C++; such declaration
	// allows the scope of this function libusb_get_device_list() to write on a variable
	// of type libusb_device** declared within the function scope of the caller.
	// libusb_device** is better understood as libusb_device*[], which is an array of
	// pointers of type libusb_device*, each pointing to a struct that holds actual data.
	usb_find_busses();
	usb_find_devices();
	usb_bus* bus = usb_get_busses();
	std::vector<struct usb_device*> vDevices;
	while (bus)
	{
		struct usb_device* device (bus->devices);
		while (device)
		{
			vDevices.push_back(device);
			device = device->next;
		};
		bus = bus->next;
	};
	*list = (libusb_device**) malloc(vDevices.size()*sizeof(struct usb_device*));
	memcpy(*list, &vDevices[0], vDevices.size()*sizeof(struct usb_device*));
	// the number of devices in the outputted list, or LIBUSB_ERROR_NO_MEM on memory allocation failure.
	return(vDevices.size());
}

void libusb_free_device_list(libusb_device** list, int unref_devices)
{
	free(list);
}

int libusb_get_device_descriptor(libusb_device* dev, struct libusb_device_descriptor*	desc)
{
	struct usb_device* device (translate(dev));
	usb_device_descriptor& device_desc (device->descriptor);
	// plain copy of one descriptor on to another: this is a safe operation because
	// usb_device_descriptor and libusb_device_descriptor have the same signature
	memcpy(desc, &device_desc, sizeof(libusb_device_descriptor));
	// 0 on success; LIBUSB_ERROR on failure
	return(0);
}

int libusb_open(libusb_device* dev, libusb_device_handle** handle)
{
	struct usb_device* device (translate(dev));
	usb_dev_handle* device_handle (usb_open(device));
	*handle = translate(device_handle);
	if (NULL == device_handle)
	{
		// LIBUSB_ERROR_NO_MEM on memory allocation failure
		// LIBUSB_ERROR_ACCESS if the user has insufficient permissions
		// LIBUSB_ERROR_NO_DEVICE if the device has been disconnected
		// another LIBUSB_ERROR code on other failure
		return(LIBUSB_ERROR_OTHER);
	}
	//0 on success
	return(0);
}

void libusb_close(libusb_device_handle*	dev_handle)
{
	usb_dev_handle* device_handle (translate(dev_handle));
	usb_close(device_handle);
}

int libusb_claim_interface(libusb_device_handle* dev, int interface_number)
{
	usb_dev_handle* device_handle (translate(dev));
	if (0 == usb_claim_interface(device_handle, interface_number))
	{
		int ok2 = usb_set_configuration(device_handle, 1);
	}
	// LIBUSB_ERROR_NOT_FOUND if the requested interface does not exist
	// LIBUSB_ERROR_BUSY if another program or driver has claimed the interface
	// LIBUSB_ERROR_NO_DEVICE if the device has been disconnected
	// a LIBUSB_ERROR code on other failure
	// 0 on success
	return(0);
}

int libusb_release_interface(libusb_device_handle* dev, int interface_number)
{
	usb_dev_handle* device_handle (translate(dev));
	usb_release_interface(device_handle, interface_number);
	// LIBUSB_ERROR_NOT_FOUND if the interface was not claimed
	// LIBUSB_ERROR_NO_DEVICE if the device has been disconnected
	// another LIBUSB_ERROR code on other failure
	// 0 on success
	return(0);
}

int libusb_control_transfer(libusb_device_handle* dev_handle, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, unsigned char* data, uint16_t wLength, unsigned int timeout)
{
	usb_dev_handle* device_handle (translate(dev_handle));
	// in libusb-1.0 a timeout of zero it means 'wait indefinitely'; in libusb-0.1, a timeout of zero means 'return immediatelly'!
	timeout = (0 == timeout) ? 60000 : timeout;   // wait 60000ms (60s = 1min) if the transfer is supposed to wait indefinitely...
	int bytes_transferred = usb_control_msg(device_handle, bmRequestType, bRequest, wValue, wIndex, (char*)data, wLength, timeout);
	// on success, the number of bytes actually transferred
	// LIBUSB_ERROR_TIMEOUT if the transfer timed out
	// LIBUSB_ERROR_PIPE if the control request was not supported by the device
	// LIBUSB_ERROR_NO_DEVICE if the device has been disconnected
	// another LIBUSB_ERROR code on other failures
	return(bytes_transferred);
}

// FROM HERE ON CODE BECOMES QUITE MESSY: ASYNCHRONOUS TRANSFERS AND HANDLE EVENTS STUFF

struct transfer_wrapper
{
	transfer_wrapper* prev;
	transfer_wrapper* next;
	void* usb;
	libusb_transfer libusb;
};

transfer_wrapper* GetWrapper(struct libusb_transfer* transfer)
{
	const char* const raw_address ((char*)transfer);
	const void* const off_address (raw_address-sizeof(void*)-2*sizeof(transfer_wrapper*));
	transfer_wrapper* wrapper ((transfer_wrapper*)off_address);
	return(wrapper);
}

template<typename T>
struct QuickList
{
	T ini;
	T end;
	QuickList()
	{
		memset(&ini, 0, sizeof(T));
		memset(&end, 0, sizeof(T));
		ini.prev = NULL;
		ini.next = &end;
		end.prev = &ini;
		end.next = NULL;
	}
	const bool Empty() const
	{
		return(ini.next == &end);  // could also be (end.prev == &ini)
	}
	void Append(T* node)
	{
		if (!Orphan(node))
			fprintf(stdout, "WARNING: Appending non-orphan node to list...\n");
		end.prev->next = node;
		node->prev = end.prev;
		node->next = &end;
		end.prev = node;
	}
	T* Head() const
	{
		T* head (NULL);
		if (!Empty())
			head = ini.next;
		return(head);
	}
	T* Last() const
	{
		T* last (NULL);
		if (!Empty())
			last = end.prev;
		return(last);
	}
	const bool Member(T* node) const
	{
		bool member (false);
		for (T* it=ini.next; it!=&end, member!=true; it=Next(it))
			if (it == node)
				member = true;
		return(member);
	}
	static T* Prev(T* node)
	{
		T* prev (NULL);
		if (NULL != node->prev->prev)
			prev = node->prev;
		return(prev);
	}
	static T* Next(T* node)
	{
		T* next (NULL);
		if (NULL != node->next->next)
			next = node->next;
		return(next);
	}
	static void Remove(T* node)
	{
		if (Orphan(node))
			return;
		node->prev->next = node->next;
		node->next->prev = node->prev;
		node->prev = NULL;
		node->next = NULL;
	}
	static const bool Orphan(T* node)
	{
		return((NULL == node->prev) && (NULL == node->next)); // (node->prev == node->next)
	}
};
typedef QuickList<transfer_wrapper> TListTransfers;

static TListTransfers limboTransfers;
typedef std::map<int, TListTransfers*> TMapIsocTransfers;
static TMapIsocTransfers mIsocTransfers;
struct libusb_transfer* libusb_alloc_transfer(int iso_packets)
{
	transfer_wrapper* wrapper = new transfer_wrapper;
	memset(wrapper, 0, sizeof(transfer_wrapper));
	libusb_transfer& transfer (wrapper->libusb);
	transfer.num_iso_packets = iso_packets;
	transfer.iso_packet_desc = new libusb_iso_packet_descriptor [iso_packets];
	memset(transfer.iso_packet_desc, 0, iso_packets*sizeof(libusb_iso_packet_descriptor));
	// a newly allocated transfer, or NULL on error
	return(&transfer);
}

void free_wrapper(transfer_wrapper* wrapper)
{
	TListTransfers::Remove(wrapper);
	usb_free_async(&wrapper->usb);
	delete[](wrapper->libusb.iso_packet_desc);
	delete(wrapper);
}

void libusb_free_transfer(struct libusb_transfer* transfer)
{
	transfer_wrapper* wrapper = GetWrapper(transfer);
	if (wrapper->libusb.flags & LIBUSB_TRANSFER_FREE_TRANSFER)
		free_wrapper(wrapper);
	else if (TListTransfers::Orphan(wrapper))
		free_wrapper(wrapper);
	else
		wrapper->libusb.flags |= LIBUSB_TRANSFER_FREE_TRANSFER;
}

void libusb_fill_iso_transfer(struct libusb_transfer* transfer, libusb_device_handle* dev_handle, unsigned char endpoint, unsigned char* buffer, int length, int num_iso_packets, libusb_transfer_cb_fn callback, void* user_data, unsigned int timeout)
{
	transfer->dev_handle = dev_handle;
	transfer->endpoint = endpoint;
	transfer->buffer = buffer;
	transfer->length = length;
	transfer->num_iso_packets = num_iso_packets;
	transfer->callback = callback;
	transfer->timeout = timeout;
	transfer->user_data = user_data;
	transfer->type = LIBUSB_TRANSFER_TYPE_ISOCHRONOUS;

	// control library duties such as: LIBUSB_TRANSFER_SHORT_NOT_OK, LIBUSB_TRANSFER_FREE_BUFFER, LIBUSB_TRANSFER_FREE_TRANSFER
	transfer->flags;
	// these two are output parameters coming from actual transfers...
	transfer->actual_length;
	transfer->status;
}

void libusb_set_iso_packet_lengths(struct libusb_transfer* transfer, unsigned int length)
{
	for (int i=0; i < transfer->num_iso_packets; ++i)
		transfer->iso_packet_desc[i].length = length;
}

int libusb_submit_transfer(struct libusb_transfer* transfer)
{
	transfer_wrapper* wrapper = GetWrapper(transfer);
	void*& context = wrapper->usb;
	if (NULL == context)
	{
		int ret (LIBUSB_ERROR_OTHER);
		switch(transfer->type)
		{
		case LIBUSB_TRANSFER_TYPE_CONTROL :
			fprintf(stderr, "ERROR: libusb_submit_transfer() with LIBUSB_TRANSFER_TYPE_CONTROL; use libusb_control_transfer() instead.\n");
			return(LIBUSB_ERROR_INVALID_PARAM);
		case LIBUSB_TRANSFER_TYPE_ISOCHRONOUS :
			ret = usb_isochronous_setup_async(translate(transfer->dev_handle), &context, transfer->endpoint, transfer->iso_packet_desc[0].length);
			break;
		case LIBUSB_TRANSFER_TYPE_BULK :
			ret = usb_bulk_setup_async(translate(transfer->dev_handle), &context, transfer->endpoint);
			break;
		case LIBUSB_TRANSFER_TYPE_INTERRUPT :
			ret = usb_interrupt_setup_async(translate(transfer->dev_handle), &context, transfer->endpoint);
			break;
		default :
			return(LIBUSB_ERROR_INVALID_PARAM);
		}
		if (ret < 0)
		{
			// TODO: better error handling...
			// what do the functions usb_***_setup_async() actually return on error?
			return(ret);
		}

		TMapIsocTransfers::iterator it = mIsocTransfers.find(transfer->endpoint);
		if (it == mIsocTransfers.end())
			mIsocTransfers.insert(std::make_pair(transfer->endpoint, new TListTransfers()));
	}

	mIsocTransfers[transfer->endpoint]->Append(wrapper);
	transfer->status = LIBUSB_TRANSFER_COMPLETED;
	int ret = usb_submit_async(context, (char*)transfer->buffer, transfer->length);
	if (ret < 0)
	{
		// TODO: better error handling...
		// what does usb_submit_async() actually returns on error?
		// LIBUSB_ERROR_NO_DEVICE if the device has been disconnected
		// LIBUSB_ERROR_BUSY if the transfer has already been submitted.
		// another LIBUSB_ERROR code on other failure
		return(ret);
	}
	// 0 on success
	return(0);
}

int libusb_cancel_transfer(struct libusb_transfer* transfer)
{
	transfer_wrapper* wrapper = GetWrapper(transfer);
	transfer->status = LIBUSB_TRANSFER_CANCELLED;
	int ret = usb_cancel_async(wrapper->usb);
	// 0 on success
	// LIBUSB_ERROR_NOT_FOUND if the transfer is already complete or cancelled.
	// a LIBUSB_ERROR code on failure
	return(ret);
}

int ReapSequential();
int ReapThreaded();

int libusb_handle_events(libusb_context* ctx)
{
	//ReapSequential();
	ReapThreaded();
	//Sleep(1);
	// 0 on success, or a LIBUSB_ERROR code on failure
	return(0);
}

int ReapTransfer(transfer_wrapper*,int=0);

int ReapSequential()
{
	// This reap strategy is very buggy and it is not working properly...
	TMapIsocTransfers::iterator it  (mIsocTransfers.begin());
	TMapIsocTransfers::iterator end (mIsocTransfers.end());
	for (; it!=end; ++it)
	{
		TListTransfers& list (*it->second);
		transfer_wrapper* wrapper (list.Head());
		if (NULL != wrapper)
			ReapTransfer(wrapper, 1000);
	}
	return(0);
}

DWORD WINAPI ReapThreadProc(__in LPVOID lpParameter)
{
	const int endpoint ((int)lpParameter);
	TListTransfers& listTransfers = *mIsocTransfers[endpoint];
	while(!listTransfers.Empty())
	{
		transfer_wrapper* wrapper = listTransfers.Head();
		if (NULL != wrapper)
			ReapTransfer(wrapper, 1000);
	}
	ExitThread(0);
	return(0);
}
int ReapThreaded()
{
	static HANDLE hThreadVideo (0);
	if ((NULL == hThreadVideo) && 
		(mIsocTransfers[0x81] != NULL) &&
		(!mIsocTransfers[0x81]->Empty()))
	{
		hThreadVideo = CreateThread(NULL, 0, ReapThreadProc, (void*)0x81, 0, NULL);
		SetThreadPriority(hThreadVideo, THREAD_PRIORITY_TIME_CRITICAL);  // THIS IS IMPORTANT!
	}
	static HANDLE hThreadDepth (0);
	if ((NULL == hThreadDepth) && 
		(mIsocTransfers[0x82] != NULL) &&
		(!mIsocTransfers[0x82]->Empty()))
	{
		hThreadDepth = CreateThread(NULL, 0, ReapThreadProc, (void*)0x82, 0, NULL);
		SetThreadPriority(hThreadDepth, THREAD_PRIORITY_TIME_CRITICAL);  // THIS IS IMPORTANT!
	}
	if (WAIT_OBJECT_0 == WaitForSingleObject(hThreadVideo, 0))
	{
		hThreadVideo = NULL;
		fprintf(stdout, "Video thread terminated.\n");
	}
	if (WAIT_OBJECT_0 == WaitForSingleObject(hThreadDepth, 0))
	{
		hThreadDepth = NULL;
		fprintf(stdout, "Depth thread terminated.\n");
	}
	// Fail Safe for THREAD_PRIORITY_TIME_CRITICAL...
	if (_kbhit())
	{
		if (_getch() == 27) // ESC
		{
			SuspendThread(hThreadVideo);
			SuspendThread(hThreadDepth);
		}
	}
	return(0);
}

void PreprocessTransferDefault(libusb_transfer* transfer, const int read);
void PreprocessTransferFreenect(libusb_transfer* transfer, const int read);
static void(*PreprocessTransfer)(libusb_transfer*, const int) (PreprocessTransferFreenect);

int ReapTransfer(transfer_wrapper* wrapper, int timeout)
{
	void* context (wrapper->usb);
	libusb_transfer* transfer (&wrapper->libusb);
	if (transfer->flags & LIBUSB_TRANSFER_FREE_TRANSFER)
	{
		libusb_free_transfer(transfer);
		return(0);
	}

	const int read = usb_reap_async_nocancel(context, timeout);
	bool processed (true);
	if (read > 0)
	{
		PreprocessTransfer(transfer, read);
	}
	else if (0 == read)
	{
		const int pkts (transfer->num_iso_packets);
		for (int i=0; i<pkts; ++i)
		{
			libusb_iso_packet_descriptor& desc (transfer->iso_packet_desc[i]);
			desc.actual_length = 0;
		}
	}
	else if (read < 0)
	{
		enum { ETIMEOUT = -116 };
		if (ETIMEOUT == read)
		{
			if (LIBUSB_TRANSFER_CANCELLED != transfer->status)
				processed = false;
		}
		else
		{
			libusb_cancel_transfer(transfer);
			processed = false;
		}
	}

	if (processed)
	{
		TListTransfers::Remove(wrapper);
		transfer->callback(transfer);
		if (read > 0)
			memset(transfer->buffer, 0, transfer->length);
	}

	return(read);
}

void PreprocessTransferDefault(libusb_transfer* transfer, const int read)
{
	unsigned int remaining (read);
	const int pkts (transfer->num_iso_packets);
	for (int i=0; i<pkts; ++i)
	{
		libusb_iso_packet_descriptor& desc (transfer->iso_packet_desc[i]);
		desc.actual_length = __min(remaining, desc.length);
		remaining -= desc.actual_length;
	}
}

void PreprocessTransferFreenect(libusb_transfer* transfer, const int read)
{
	fnusb_isoc_stream* xferstrm = (fnusb_isoc_stream*)transfer->user_data;
	freenect_device* dev = xferstrm->parent->parent;
	packet_stream* pktstrm = (transfer->endpoint == 0x81) ? &dev->video : &dev->depth;

	// Kinect Camera Frame Packed Header:
	struct pkt_hdr
	{
		uint8_t magic[2];
		uint8_t pad;
		uint8_t flag;
		uint8_t unk1;
		uint8_t seq;
		uint8_t unk2;
		uint8_t unk3;
		uint32_t timestamp;
	};  // 12 bytes

	//packet sizes:
	//          first  middle  last
	// Bayer    1920    1920    24
	// IR       1920    1920  1180
	// YUV422   1920    1920    36
	// Depth    1760    1760  1144
	const unsigned int pktlen = sizeof(pkt_hdr) + pktstrm->pkt_size;
	const unsigned int pktend = sizeof(pkt_hdr) + pktstrm->last_pkt_size;

	unsigned int remaining (read);
	unsigned int leftover (transfer->length);
	unsigned char* pktbuffer (transfer->buffer);
	const int pkts (transfer->num_iso_packets);
	for (int i=0; i<pkts; ++i)
	{
		const pkt_hdr& header (*(pkt_hdr*)pktbuffer);
		libusb_iso_packet_descriptor& desc (transfer->iso_packet_desc[i]);
		if ((header.magic[0] == 'R') && (header.magic[1] == 'B'))
		{
			switch(header.flag & 0x0F)
			{
			case 0x01 : // begin
			case 0x02 : // middle
				desc.actual_length = __min(remaining, pktlen);
				break;
			case 0x05 : // final
				desc.actual_length = __min(remaining, pktend);
				break;
			default :
				fprintf(stdout, "0x%02X\n", header.flag);
				break;
			}
		}
		else
		{
			desc.actual_length = 0;
		}
		remaining -= desc.actual_length;
		pktbuffer += desc.length;   // a.k.a: += 1920
		leftover  -= desc.length;   // a.k.a: -= 1920
	}

	if (remaining > 0)
	{
		fprintf(stdout, "%d remaining out of %d\n", remaining, read);
		if (remaining == read)
		{
		}
	}
}
