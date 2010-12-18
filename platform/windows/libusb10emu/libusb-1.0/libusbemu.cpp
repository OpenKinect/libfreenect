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
#include <set>
#include <map>
#include <algorithm>
#include <windows.h>
#include <conio.h>
#include "freenect_internal.h"

// stick to the Windows mutexes for now, but this structure could be easily
// replaced by pthread mutexes for portability or even by dummy mutexes for
// a much lightweight run-time, provided that the library client is careful
// enough to avoid any race-conditions...
struct QuickMutex
{
  CRITICAL_SECTION cs;
  QuickMutex()  { InitializeCriticalSection(&cs); }
  ~QuickMutex() { DeleteCriticalSection(&cs); }
  inline void Enter() { EnterCriticalSection(&cs); }
  inline void Leave() { LeaveCriticalSection(&cs); }
};

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
  // copy-constructor is required to be safely used as a plain std::map value
	QuickList(const QuickList& rhs)
	{
    if (!rhs.Empty())
    {
      fprintf(stdout, "WARNING: Copy-constructin from a non-empty QuickList!\n");
      return;
    }
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
    {
      fprintf(stdout, "WARNING: Appending non-orphan node to list...\n");
      Remove(node);
    }
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

struct transfer_wrapper
{
	transfer_wrapper* prev;
	transfer_wrapper* next;
	void* usb;
	libusb_transfer libusb;
};

typedef QuickList<transfer_wrapper> TListTransfers;

typedef std::map<int, TListTransfers> TMapIsocTransfers;

struct libusb_device_t
{
	libusb_context* ctx;
  struct usb_device* device;
  int refcount;
  TMapIsocTransfers* isoTransfers;
  // comparator required for unique-key STL containers such as std::set
  bool operator < (const libusb_device& rhs) const
  {
    return(device < rhs.device);
  }
};

struct libusb_device_handle_t
{
	libusb_device* dev;
  usb_dev_handle* handle;
};

struct libusb_context_t
{
  std::set<libusb_device> devices;
};

int libusb_init(libusb_context** context)
{
	usb_init();
	// there is no such a thing like 'context' in libusb-0.1...
	// however, it is wise to emulate such context structure to localize and
  // keep track of any resource and/or internal data structures, as well as
  // to be able to clean-up itself at libusb_exit()
	*context = new libusb_context;
	// 0 on success; LIBUSB_ERROR on failure
	return(0);
}

void libusb_exit(libusb_context* ctx)
{
  // TODO: sweep the context contents and clean-up anything that was not
  // already explicitly released by the cliend code...
	delete(ctx);
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
	while (bus)
	{
		struct usb_device* device = bus->devices;
		while (device)
		{
      // TODO: find a more elegant and less dangerous way to refcount++...
      libusb_device key = { ctx, device, 0, NULL };
      std::set<libusb_device>::iterator it = ctx->devices.insert(key).first;
      libusb_device& record = (libusb_device&)(*it);
      record.refcount++;
			device = device->next;
		};
		bus = bus->next;
	};
  *list = new libusb_device* [ctx->devices.size()+1];
  std::set<libusb_device>::iterator it  (ctx->devices.begin());
  std::set<libusb_device>::iterator end (ctx->devices.end());
  libusb_device**& devlist = *list;
  for (int i=0; it!=end; ++it, ++i)
  {
    // TODO: find a more elegant and safer way to get the pointers...
    const libusb_device& wrapper (*it);
    devlist[i] = (libusb_device*)&wrapper;
  }
  devlist[ctx->devices.size()] = NULL;
	// the number of devices in the outputted list, or LIBUSB_ERROR_NO_MEM on memory allocation failure.
	return(ctx->devices.size());
}

void libusb_free_device_list(libusb_device** list, int unref_devices)
{
  if (NULL == list)
    return;
  if (unref_devices)
  {
    int i (0);
    while (list[i] != NULL)
    {
      list[i]->refcount--;
      ++i;
    }
  }
  // TODO: remove devices from context if refcount is zero...
	delete[](list);
}

int libusb_get_device_descriptor(libusb_device* dev, struct libusb_device_descriptor*	desc)
{
  struct usb_device* device (dev->device);
	usb_device_descriptor& device_desc (device->descriptor);
	// plain copy of one descriptor on to another: this is a safe operation because
	// usb_device_descriptor and libusb_device_descriptor have the same signature
	memcpy(desc, &device_desc, sizeof(libusb_device_descriptor));
	// 0 on success; LIBUSB_ERROR on failure
	return(0);
}

int libusb_open(libusb_device* dev, libusb_device_handle** handle)
{
  usb_dev_handle* usb_handle (usb_open(dev->device));
  if (NULL == usb_handle)
    return(LIBUSB_ERROR_OTHER);
  *handle = new libusb_device_handle;
  (*handle)->dev = dev;
  (*handle)->handle = usb_handle;
  dev->refcount++;
  if (NULL == dev->isoTransfers)
    dev->isoTransfers = new TMapIsocTransfers;
	//0 on success
	// LIBUSB_ERROR_NO_MEM on memory allocation failure
	// LIBUSB_ERROR_ACCESS if the user has insufficient permissions
	// LIBUSB_ERROR_NO_DEVICE if the device has been disconnected
	// another LIBUSB_ERROR code on other failure
	return(0);
}

void libusb_close(libusb_device_handle*	dev_handle)
{
  usb_close(dev_handle->handle);
  dev_handle->dev->refcount--;
  // TODO: remove devices from context if refcount is zero...
  delete(dev_handle);
}

int libusb_claim_interface(libusb_device_handle* dev, int interface_number)
{
	if (0 != usb_claim_interface(dev->handle, interface_number))
    return(LIBUSB_ERROR_OTHER);
  if (0 != usb_set_configuration(dev->handle, 1))
    return(LIBUSB_ERROR_OTHER);
	// LIBUSB_ERROR_NOT_FOUND if the requested interface does not exist
	// LIBUSB_ERROR_BUSY if another program or driver has claimed the interface
	// LIBUSB_ERROR_NO_DEVICE if the device has been disconnected
	// a LIBUSB_ERROR code on other failure
	// 0 on success
	return(0);
}

int libusb_release_interface(libusb_device_handle* dev, int interface_number)
{
	if (0 != usb_release_interface(dev->handle, interface_number))
    return(LIBUSB_ERROR_OTHER);
	// LIBUSB_ERROR_NOT_FOUND if the interface was not claimed
	// LIBUSB_ERROR_NO_DEVICE if the device has been disconnected
	// another LIBUSB_ERROR code on other failure
	// 0 on success
	return(0);
}

int libusb_control_transfer(libusb_device_handle* dev_handle, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, unsigned char* data, uint16_t wLength, unsigned int timeout)
{
	// in libusb-1.0 a timeout of zero it means 'wait indefinitely'; in libusb-0.1, a timeout of zero means 'return immediatelly'!
	timeout = (0 == timeout) ? 60000 : timeout;   // wait 60000ms (60s = 1min) if the transfer is supposed to wait indefinitely...
	int bytes_transferred = usb_control_msg(dev_handle->handle, bmRequestType, bRequest, wValue, wIndex, (char*)data, wLength, timeout);
	// on success, the number of bytes actually transferred
	// LIBUSB_ERROR_TIMEOUT if the transfer timed out
	// LIBUSB_ERROR_PIPE if the control request was not supported by the device
	// LIBUSB_ERROR_NO_DEVICE if the device has been disconnected
	// another LIBUSB_ERROR code on other failures
	return(bytes_transferred);
}

// FROM HERE ON CODE BECOMES QUITE MESSY: ASYNCHRONOUS TRANSFERS AND HANDLE EVENTS STUFF

transfer_wrapper* GetWrapper(struct libusb_transfer* transfer)
{
	const char* const raw_address ((char*)transfer);
	const void* const off_address (raw_address-sizeof(void*)-2*sizeof(transfer_wrapper*));
	transfer_wrapper* wrapper ((transfer_wrapper*)off_address);
	return(wrapper);
}

static int cnt (0);
struct libusb_transfer* libusb_alloc_transfer(int iso_packets)
{
  transfer_wrapper* wrapper = new transfer_wrapper;
  memset(wrapper, 0, sizeof(transfer_wrapper));
  libusb_transfer& transfer (wrapper->libusb);
  transfer.num_iso_packets = iso_packets;
  transfer.iso_packet_desc = new libusb_iso_packet_descriptor [iso_packets];
  memset(transfer.iso_packet_desc, 0, iso_packets*sizeof(libusb_iso_packet_descriptor));
  ++cnt;
  // a newly allocated transfer, or NULL on error
  return(&transfer);
}

void libusb_free_transfer(struct libusb_transfer* transfer)
{
  // according to the official libusb_free_transfer() documentation:
  // "It is legal to call this function with a NULL transfer.
  //  In this case, the function will simply return safely."
  if (NULL == transfer)
    return;

  // according to the official libusb_free_transfer() documentation:
  // "It is not legal to free an active transfer
  //  (one which has been submitted and has not yet completed)."
  // that means that only "orphan" transfers can be deleted:
  transfer_wrapper* wrapper = GetWrapper(transfer);
  if (!TListTransfers::Orphan(wrapper))
  {
    fprintf(stderr, "ERROR: libusb_free_transfer() attempted to free an active transfer!\n");
    return;
  }

  // according to the official libusb_free_transfer() documentation:
  // "If the LIBUSB_TRANSFER_FREE_BUFFER flag is set and the transfer buffer
  //  is non-NULL, this function will also free the transfer buffer using the
  //  standard system memory allocator (e.g. free())."
  usb_free_async(&wrapper->usb);
  if (transfer->flags & LIBUSB_TRANSFER_FREE_BUFFER)
    free(transfer->buffer);
  delete[](transfer->iso_packet_desc);
  memset(wrapper, 0, sizeof(transfer_wrapper)); // Paranoid clean...
  delete(wrapper);

  --cnt;
  fprintf(stdout, "transfer deleted; %d remaining\n", cnt);
}

void libusb_fill_iso_transfer(struct libusb_transfer* transfer, libusb_device_handle* dev_handle, unsigned char endpoint, unsigned char* buffer, int length, int num_iso_packets, libusb_transfer_cb_fn callback, void* user_data, unsigned int timeout)
{
  // according to the official libusb_fill_iso_transfer() documentation:
  // "libusb_fill_iso_transfer() is a helper function to populate the required
  //  libusb_transfer fields for an isochronous transfer."
  // What this means is that the library client is not required to call this
  // helper function in order to setup the fields within the libusb_transfer
  // struct. Thus, this is NOT the place for any sort of special processing
  // because there are no guarantees that such function will ever be invoked.
	transfer->dev_handle = dev_handle;
	transfer->endpoint = endpoint;
	transfer->buffer = buffer;
	transfer->length = length;
	transfer->num_iso_packets = num_iso_packets;
	transfer->callback = callback;
	transfer->timeout = timeout;
	transfer->user_data = user_data;
	transfer->type = LIBUSB_TRANSFER_TYPE_ISOCHRONOUS;

	// control some additonal library duties such as:
  // LIBUSB_TRANSFER_SHORT_NOT_OK, LIBUSB_TRANSFER_FREE_BUFFER, LIBUSB_TRANSFER_FREE_TRANSFER
	transfer->flags;
	// these two are output parameters coming from actual transfers...
	transfer->actual_length;
	transfer->status;
}

void libusb_set_iso_packet_lengths(struct libusb_transfer* transfer, unsigned int length)
{
  // according to the official libusb_fill_iso_transfer() documentation:
  // "Convenience function to set the length of all packets in an isochronous
  //  transfer, based on the num_iso_packets field in the transfer structure."
  // For the same reasons as in libusb_fill_iso_transfer(), no additional
  // processing should ever happen withing this function...
	for (int i=0; i < transfer->num_iso_packets; ++i)
		transfer->iso_packet_desc[i].length = length;
}

int SetupTransfer(transfer_wrapper* wrapper)
{
  void*& context = wrapper->usb;
  if (NULL != context)  // Paranoid check...
    return(LIBUSB_ERROR_OTHER);
  
  int ret (LIBUSB_ERROR_OTHER);
  libusb_transfer* transfer = &wrapper->libusb;
  switch(transfer->type)
  {
    case LIBUSB_TRANSFER_TYPE_ISOCHRONOUS :
      ret = usb_isochronous_setup_async(transfer->dev_handle->handle, &context, transfer->endpoint, transfer->iso_packet_desc[0].length);
      break;
    case LIBUSB_TRANSFER_TYPE_CONTROL :
      // libusb-0.1 does not actually support asynchronous control transfers, but this should be
      // very easy to emulate if necessary: just stick the transfer in a special list and then
      // libusb_handle_events() check if the list is empty or not; if it is not empty, a thread
      // is created temporarily just to deal with such control transfer requests until the list
      // becomes eventually empty again and the thread terminates.
    case LIBUSB_TRANSFER_TYPE_BULK :
    case LIBUSB_TRANSFER_TYPE_INTERRUPT :
      // these transfer types are not being used by libfreenect. they should be fairly simple to
      // emulate with libusb-0.1 since it already provides support for them.
      // usb_bulk_setup_async(translate(transfer->dev_handle), &context, transfer->endpoint);
      // usb_interrupt_setup_async(translate(transfer->dev_handle), &context, transfer->endpoint);
    default :
      return(LIBUSB_ERROR_INVALID_PARAM);
  }

  if (ret < 0)
  {
    // TODO: better error handling...
    // what do the functions usb_***_setup_async() actually return on error?
    return(ret);
  }

  return(LIBUSB_SUCCESS);
}

int libusb_submit_transfer(struct libusb_transfer* transfer)
{
  transfer_wrapper* wrapper = GetWrapper(transfer);

  // the first time a transfer is submitted, the libusb-0.1 transfer context
  // (the void*) must be created and initialized with a proper call to one of
  // the usb_***_setup_async() functions; one could thing of doing this setup
  // within libusb_fill_***_transfer(), but the latter are just convenience
  // functions to fill the transfer data structure: the library client is not
  // forced to call them and could fill the fields directly within the struct.
  if (NULL == wrapper->usb)
    SetupTransfer(wrapper);

  TMapIsocTransfers& isoTransfers (*transfer->dev_handle->dev->isoTransfers);
  isoTransfers[transfer->endpoint].Append(wrapper);
  transfer->status = LIBUSB_TRANSFER_COMPLETED;
  int ret = usb_submit_async(wrapper->usb, (char*)transfer->buffer, transfer->length);
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
  // according to the official libusb_cancel_transfer() documentation:
  // "This function returns immediately, but this does not indicate
  //  cancellation is complete. Your callback function will be invoked at
  //  some later time with a transfer status of LIBUSB_TRANSFER_CANCELLED."
  // This semantic can be emulated by setting the transfer->status flag to
  // LIBUSB_TRANSFER_CANCELLED, leaving the rest to libusb_handle_events().
	transfer->status = LIBUSB_TRANSFER_CANCELLED;
	int ret = usb_cancel_async(wrapper->usb);
	// 0 on success
	// LIBUSB_ERROR_NOT_FOUND if the transfer is already complete or cancelled.
	// a LIBUSB_ERROR code on failure
	return(ret);
}

int ReapSequential(const libusb_device&);
int ReapThreaded(const libusb_device&);
static int(*ReapStrategy)(const libusb_device&) (ReapThreaded);

int libusb_handle_events(libusb_context* ctx)
{
  std::set<libusb_device>::iterator it  (ctx->devices.begin());
  std::set<libusb_device>::iterator end (ctx->devices.end());
  for (; it!=end; ++it)
  {
    const libusb_device& dev (*it);
    if (dev.refcount > 0)
      ReapStrategy(dev);
  }

  // Fail Safe for THREAD_PRIORITY_TIME_CRITICAL: press ESC key on the console window...
  if (_kbhit())
    if (27 == _getch()) // ESC
    {
      fprintf(stderr, "libusb_handle_events() fail guard reached!\n");
      return(LIBUSB_ERROR_INTERRUPTED);
    }

  // 0 on success, or a LIBUSB_ERROR code on failure
  return(0);
}

int ReapTransfer(transfer_wrapper*,int=0);

int ReapSequential(const libusb_device& dev)
{
  static QuickMutex mutex;
  mutex.Enter();
	// This reap strategy is very buggy and it is not working properly...
  TMapIsocTransfers::iterator it  (dev.isoTransfers->begin());
	TMapIsocTransfers::iterator end (dev.isoTransfers->end());
	for (; it!=end; ++it)
	{
    TListTransfers& list (it->second);
		transfer_wrapper* wrapper (list.Head());
		if (NULL != wrapper)
			ReapTransfer(wrapper, 1000);
	}
  mutex.Leave();
  //Sleep(1);
	return(0);
}

DWORD WINAPI ReapThreadProc(__in LPVOID lpParameter)
{
  TListTransfers& listTransfers (*(TListTransfers*)lpParameter);
	while(!listTransfers.Empty())
	{
		transfer_wrapper* wrapper = listTransfers.Head();
		if (NULL != wrapper)
			ReapTransfer(wrapper, 1000);
	}
	ExitThread(0);
	return(0);
}
int ReapThreaded(const libusb_device& dev)
{
  static std::map<const libusb_device*, std::map<int,HANDLE> > mapDeviceEndPointThreads;
  TMapIsocTransfers::iterator it  (dev.isoTransfers->begin());
	TMapIsocTransfers::iterator end (dev.isoTransfers->end());
  for (; it!=end; ++it)
  {
    const int endpoint (it->first);
    HANDLE& hThread = mapDeviceEndPointThreads[&dev][endpoint];
    if (WAIT_OBJECT_0 == WaitForSingleObject(hThread, 0))
      hThread = NULL;
    if (NULL == hThread)
      if (!it->second.Empty())
      {
        hThread = CreateThread(NULL, 0, ReapThreadProc, (void*)&(it->second), 0, NULL);
        SetThreadPriority(hThread, THREAD_PRIORITY_TIME_CRITICAL);  // THIS IS IMPORTANT!
      }
  }
  Sleep(1);
	return(0);
}

void PreprocessTransferNaive(libusb_transfer* transfer, const int read);
void PreprocessTransferFreenect(libusb_transfer* transfer, const int read);
static void(*PreprocessTransfer)(libusb_transfer*, const int) (PreprocessTransferFreenect);

int ReapTransfer(transfer_wrapper* wrapper, int timeout)
{
  void* context (wrapper->usb);
  libusb_transfer* transfer (&wrapper->libusb);

	const int read = usb_reap_async_nocancel(context, timeout);
  if (read >= 0)
  {
    // data successfully acquired (0 bytes is also a go!), which means that
    // the transfer should be removed from the head of the list and put into
    // an orphan state.
    TListTransfers::Remove(wrapper);

    // if data is effectively acquired (non-zero bytes transfer), all of the
    // associated iso packed descriptors must be filled properly; this is an
    // application specific task and requires knowledge of the logic behind
    // the streams being transferred: PreprocessTransfer() is an user-defined
    // "library-injected" routine that should perform this task.
    if (read > 0)
      PreprocessTransfer(transfer, read);

    // callback the library client through the callback; at this point, the
    // client is assumed to do whatever it wants to the data and, possibly,
    // resubmitting the transfer, which would then place the transfer at the
    // end of its related asynchronous list (orphan transfer is adopted).
    transfer->callback(transfer);

    // if data is effectively acquired (non-zero bytes transfer), it safe to
    // assume that the client allegedly used that data; what remains to be
    // done is to set the data buffer - and the iso packed descriptors - to
    // some reliable state for the future...
    if (read > 0)
    {
      memset(transfer->buffer, 0, transfer->length);
      for (int i=0; i<transfer->num_iso_packets; ++i)
        transfer->iso_packet_desc[i].actual_length = 0;
    }
  }
	else
	{
    enum { ETIMEOUT = -116 };
    switch(read)
    {
      // When usb_reap_async_nocancel() returns ETIMEOUT, two things
      // could have happened:
      // (a) the timeout passed to usb_reap_async_nocancel() really expired;
      // (b) the transfer was cancelled via usb_cancel_async().
      // In case of (a), the transfer must remain as the head of the list
      // (do not remove the node) and just return without calling back (well,
      // it might be a good idea to set the status to LIBUSB_TRANSFER_TIMED_OUT
      // and then callback...) MORE INVESTIGATION NEEDED!
      // In case of (b), the transfer was cancelled and it should be removed
      // from the list and reported through the callback.
      case ETIMEOUT :
        if (LIBUSB_TRANSFER_CANCELLED == transfer->status)
        {
          TListTransfers::Remove(wrapper);
          transfer->callback(transfer);
        }
        break;
      default :
        // I have not stumbled into any other negative value coming from the
        // usb_reap_async_nocancel()... Anyway, cancel seems to be a simple yet
        // plausible preemptive approach... MORE INVESTIGATION NEEDED!
        libusb_cancel_transfer(transfer);
        break;
    }
	}

  return(read);
}

// Naive transfer->iso_packet_desc array filler. It will probably never work
// with any device, but it serves as a template and as a default handler...
void PreprocessTransferNaive(libusb_transfer* transfer, const int read)
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

// This is were the transfer->iso_packet_desc array is built. Knowledge of
// the underlying device stream protocol is required in order to properly
// setup this array. Moreover, it is also necessary to sneak into some of
// the libfreenect internals so that the proper length of each iso packet
// descriptor can be inferred. Fortunately, libfreenect has this information
// avaliable in the "transfer->user_data" field which holds a pointer to a
// fnusb_isoc_stream struct with all the information required in there.
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

#ifdef _DEBUG
	if (remaining > 0)
	{
		fprintf(stdout, "%d remaining out of %d\n", remaining, read);
		if (remaining == read)
      fprintf(stdout, "no bytes consumed!\n");
	}
#endif//_DEBUG
}
