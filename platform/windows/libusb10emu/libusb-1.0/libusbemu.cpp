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

// Headers required to enable Visual C++ Memory Leak mechanism:
// (NOTE: this should only be activated in Debug mode!)
#if defined(_MSC_VER) && defined(_DEBUG)

  // MSVC version predefined macros:
  // http://social.msdn.microsoft.com/Forums/en-US/vcgeneral/thread/17a84d56-4713-48e4-a36d-763f4dba0c1c/
  // _MSC_VER | MSVC Edition
  // ---------+---------------
  //     1300 | MSVC .NET 2002
  //     1310 | MSVC .NET 2003
  //     1400 | MSVC 2005
  //     1500 | MSVC 2008
  //     1600 | MSVC 2010

  // until VC71 (MSVC2003) the human-readable map/dump mode flag is CRTDBG_MAP_ALLOC:
  // http://msdn.microsoft.com/en-us/library/x98tx3cf(v=vs.71).aspx
  // but from VC80 (MSVC2005) and on the flag should be prefixed by a "_":
  // http://msdn.microsoft.com/en-us/library/x98tx3cf(v=vs.80).aspx
  #if _MSC_VER >= 1400
    #define _CRTDBG_MAP_ALLOC
  #else
    #define CRTDBG_MAP_ALLOC
  #endif
  #include <stdlib.h>
  #include <crtdbg.h>

  // Also, gotta redefine the new operator ...
  // http://support.microsoft.com/kb/140858
  // but before redefining it, for MSVC version prior to 2010, the STL <map>
  // header must be included in order to avoid some scary compilation issues:
  // http://social.msdn.microsoft.com/forums/en-US/vclanguage/thread/a6a148ed-aff1-4ec0-95d2-a82cd4c29cbb
  #if _MSC_VER < 1600
    #include <map>
  #endif
  // now it is safe to redefine the new operator
  #define LIBUSBEMU_DEBUG_NEW  new ( _NORMAL_BLOCK, __FILE__, __LINE__)
  #define new LIBUSBEMU_DEBUG_NEW

#endif

#include "libusb.h"
#include "libusbemu_internal.h"
#include <cassert>
#include <algorithm>
#include <conio.h>
#include "freenect_internal.h"

using namespace libusbemu;

int libusb_init(libusb_context** context)
{
	usb_init();
  const usb_version* version = usb_get_version();
  fprintf(stdout, "libusb-win32 version %d.%d.%d.%d (driver %d.%d.%d.%d)\n",
    version->dll.major, version->dll.minor, version->dll.micro, version->dll.nano,
    version->driver.major, version->driver.minor, version->driver.micro, version->driver.nano);
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
  // before deleting the context, delete all devices/transfers still in there:
  while (!ctx->devices.empty())
  {
    libusb_context::TMapDevices::iterator itDevice (ctx->devices.begin());
    libusb_device& device (itDevice->second);
    if (NULL != device.handles)
    {
      // a simple "while(!device.handles->empty())" loop is impossible here
      // because after a call to libusb_close() the device may be already
      // destroyed (that's the official libusb-1.0 semantics when the ref
      // count reaches zero), rendering "device.handles" to a corrupt state.
      // an accurate "for" loop on the number of handles is the way to go.
      const int handles = device.handles->size();
      for (int h=0; h<handles; ++h)
      {
        libusb_device::TMapHandles::iterator itHandle = device.handles->begin();
        libusb_device_handle* handle (&(itHandle->second));
        libusb_close(handle);
      }
    }
  }
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
      libusbemu_register_device(ctx, device);
			device = device->next;
		};
		bus = bus->next;
	};
  // populate the device list that will be returned to the client:
  // the list must be NULL-terminated to follow the semantics of libusb-1.0!
  libusb_device**& devlist = *list;
  devlist = new libusb_device* [ctx->devices.size()+1];   // +1 is for a finalization mark
  libusb_context::TMapDevices::iterator it  (ctx->devices.begin());
  libusb_context::TMapDevices::iterator end (ctx->devices.end());
  for (int i=0; it!=end; ++it, ++i)
    devlist[i] = &it->second;
  // finalization mark to assist later calls to libusb_free_device_list()
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
      libusbemu_unregister_device(list[i]);
      ++i;
    }
  }
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

  if (NULL == dev->handles)
    dev->handles = new libusb_device::TMapHandles;

  libusb_device_handle_t dummy = { dev, usb_handle };
  libusb_device::TMapHandles::iterator it = dev->handles->insert(std::make_pair(usb_handle,dummy)).first;
  *handle = &(it->second);
  assert((*handle)->dev == dev);
  assert((*handle)->handle == usb_handle);
  dev->refcount++;

  if (NULL == dev->isoTransfers)
    dev->isoTransfers = new libusb_device::TMapIsocTransfers;

	//0 on success
	// LIBUSB_ERROR_NO_MEM on memory allocation failure
	// LIBUSB_ERROR_ACCESS if the user has insufficient permissions
	// LIBUSB_ERROR_NO_DEVICE if the device has been disconnected
	// another LIBUSB_ERROR code on other failure
	return(0);
}

void libusb_close(libusb_device_handle*	dev_handle)
{
  libusb_device* device (dev_handle->dev);
  if (device->handles->find(dev_handle->handle) == device->handles->end())
  {
    fprintf(stderr, "libusb_close() attempted to close an unregistered handle...\n");
    return;
  }
  usb_close(dev_handle->handle);
  device->handles->erase(dev_handle->handle);
  libusbemu_unregister_device(device);
}

int libusb_claim_interface(libusb_device_handle* dev, int interface_number)
{
  // according to the official libusb-win32 usb_set_configuration() documentation:
  // http://sourceforge.net/apps/trac/libusb-win32/wiki/libusbwin32_documentation
  // "Must be called!: usb_set_configuration() must be called with a valid
  //  configuration (not 0) before you can claim the interface. This might
  //  not be be necessary in the future. This behavior is different from
  //  Linux libusb-0.1."
  if (0 != usb_set_configuration(dev->handle, 1))
    return(LIBUSB_ERROR_OTHER);
	if (0 != usb_claim_interface(dev->handle, interface_number))
    return(LIBUSB_ERROR_OTHER);
  /*
  if (0 != usb_set_altinterface(dev->handle, 0))
    return(LIBUSB_ERROR_OTHER);
  */
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

// FROM HERE ON CODE BECOMES QUITE MESSY: ASYNCHRONOUS TRANSFERS MANAGEMENT

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
  transfer_wrapper* wrapper = libusbemu_get_transfer_wrapper(transfer);
  if (!libusb_device::TListTransfers::Orphan(wrapper))
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

int libusb_submit_transfer(struct libusb_transfer* transfer)
{
  transfer_wrapper* wrapper = libusbemu_get_transfer_wrapper(transfer);

  // the first time a transfer is submitted, the libusb-0.1 transfer context
  // (the void*) must be created and initialized with a proper call to one of
  // the usb_***_setup_async() functions; one could thing of doing this setup
  // within libusb_fill_***_transfer(), but the latter are just convenience
  // functions to fill the transfer data structure: the library client is not
  // forced to call them and could fill the fields directly within the struct.
  if (NULL == wrapper->usb)
    libusbemu_setup_transfer(wrapper);

  libusb_device::TMapIsocTransfers& isoTransfers (*transfer->dev_handle->dev->isoTransfers);
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
  transfer_wrapper* wrapper = libusbemu_get_transfer_wrapper(transfer);
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

// FROM HERE ON CODE BECOMES REALLY REALLY REALLY MESSY: HANDLE EVENTS STUFF

int ReapSequential(const libusb_device&);         // EXPERIMENTAL
int ReapJohnnieWalker(const libusb_device& dev);  // EXPERIMENTAL
int ReapThreaded(const libusb_device&);           // WORKS FINE

static QuickEvent hProblem;
static QuickEvent hReaction;
static QuickEvent hAbort;

int libusb_handle_events(libusb_context* ctx)
{
  int ret (0);

  int(*ReapStrategy)(const libusb_device&) (ReapThreaded);

  // ReapThreaded() is already spawning threads (one per stream) at very high
  // (time-critical) priority; the other stream reap strategies perform more
  // sequentially and the thread must be promoted to higher priority in order
  // to alleviate sequence losses; however, even at such extreme conditions,
  // sequence losses still happen at frequent pace without ReapThreaded().
  if (ReapStrategy != ReapThreaded)
    QuickThread::Myself().RaisePriority();

  //HANDLE hMyself (GetCurrentThread());
  libusb_context::TMapDevices::iterator it  (ctx->devices.begin());
  libusb_context::TMapDevices::iterator end (ctx->devices.end());
  for (; it!=end; ++it)
  {
    const libusb_device& dev (it->second);
    if (dev.refcount > 0)
      ReapStrategy(dev);
  }

  // Fail Guard to prevent THREAD_PRIORITY_TIME_CRITICAL from rendering the
  // system unresponsive: press ESC key on the CONSOLE window to kill the
  // thread; if the reap strategy is ReapThreaded(), this will also kill all
  // of the children threads internally spawned from within it.
  if (_kbhit())
    if (27 == _getch()) // ESC
      hProblem.Signal();
  if (hAbort.Check())
    ret = LIBUSB_ERROR_INTERRUPTED;
  else if (hProblem.Check())
  {
    hReaction.Reset();
    int user_option =
    MessageBoxA(GetDesktopWindow(),
                "The libusb_handle_events() fail guard of libusbemu was reached!\n"
                "This was caused by pressing the [ESC] key on the console window.\n"
                "If it was unintentional, click Cancel to resume normal execution;\n"
                "otherwise, click OK to effectively terminate the thread (note that\n"
                "the host program might run abnormally after such termination).",
                "WARNING: libusbemu thread fail guard reached!", MB_ICONWARNING | MB_OKCANCEL);
    if (IDOK == user_option)
      hAbort.Signal();
    else
      hProblem.Reset();
    hReaction.Signal();
  }

  // 0 on success, or a LIBUSB_ERROR code on failure
  return(ret);
}

enum EReapResult { ETIMEOUT = -116, ECANCELLED = -998 };
int ReapTransfer(transfer_wrapper*,int=0,libusb_device::TListTransfers* = NULL);

// EXPERIMENTAL:
// ReapSequential Rationale: reap only the head of each transfer list (stream)
int ReapSequential(const libusb_device& dev)
{
	// This reap strategy is very buggy and it is not working properly...
  libusb_device::TMapIsocTransfers::iterator it  (dev.isoTransfers->begin());
	libusb_device::TMapIsocTransfers::iterator end (dev.isoTransfers->end());
	for (; it!=end; ++it)
	{
    libusb_device::TListTransfers& list (it->second);
		transfer_wrapper* wrapper (list.Head());
		if (NULL != wrapper)
			ReapTransfer(wrapper, 0); // zero-timeout to avoid streaming to stop coming
	}
  //QuickThread::Yield();
	return(0);
}

// EXPERIMENTAL: Keep walking, Johnnie Walker...
// ReapJohnnieWalker Rationale: similar to ReapSequential, but instead of
// processing only the head of the streams at a time, the list is sweeped
// until a timeout is reached (hence the "Johnnie Walker, keep walking" :-)
// which then switches to the next stream where the same process repeats.
int ReapJohnnieWalker(const libusb_device& dev)
{
  libusb_device::TMapIsocTransfers::iterator it  (dev.isoTransfers->begin());
	libusb_device::TMapIsocTransfers::iterator end (dev.isoTransfers->end());
	for (; it!=end; ++it)
	{
    libusb_device::TListTransfers& list (it->second);
    while (!list.Empty())
    {
      transfer_wrapper* wrapper (list.Head());
      if (0 != ReapTransfer(wrapper, 0))  // zero-timeout to avoid streaming to stop coming
        break;
    }
	}
  return(0);
}

#include <list>
static QuickMutex mutexReady;   // producer-consumer stuff
static std::map<libusb_device::TListTransfers*,libusb_device::TListTransfers*> mapDeviceTransfersReady;
int ReapThreadProc(void* lpParameter)
{
  fprintf(stdout, "Thread execution started.\n");
  libusb_device::TListTransfers& listTransfers (*(libusb_device::TListTransfers*)lpParameter);

  mutexReady.Enter();
  assert(mapDeviceTransfersReady.find(&listTransfers) == mapDeviceTransfersReady.end());
  mapDeviceTransfersReady[&listTransfers] = new libusb_device::TListTransfers;
  libusb_device::TListTransfers& lstReady = *mapDeviceTransfersReady[&listTransfers];
  mutexReady.Leave();

  while(!listTransfers.Empty() || !lstReady.Empty())
	{
    if (!listTransfers.Empty())
    {
		  transfer_wrapper* wrapper = listTransfers.Head();
		  if (NULL != wrapper)
			  //ReapTransfer(wrapper, 10000, &lstReady);  // producer-consumer model
        ReapTransfer(wrapper, 10000);
    }
    else
    {
      // This is important! Otherwise the thread may "take control" of the CPU
      // if it happens to be running at TIME_CRITICAL priority...
      fprintf(stdout, "ReapThreadProc(): nothing to do, sleeping...\n");
      QuickThread::Yield();
    }

    if (hProblem.Check())
    {
      fprintf(stderr, "Thread is waiting for user reaction...\n");
      // wait for user reaction...
      hReaction.Wait();
      // did the user decide to abort?
      if (hAbort.Check())
      {
        fprintf(stderr, "Thread is aborting: releasing transfers...\n");
        while(!listTransfers.Empty())
        {
          transfer_wrapper* wrapper = listTransfers.Head();
          libusb_cancel_transfer(&(wrapper->libusb));
          ReapTransfer(wrapper, 0);
        }
        fprintf(stderr, "Thread execution aborted.\n");
        return(LIBUSB_ERROR_INTERRUPTED);
      }
    }
	}

  mutexReady.Enter();
  SAFE_DELETE(mapDeviceTransfersReady[&listTransfers]);
  mapDeviceTransfersReady.erase(&listTransfers);
  mutexReady.Leave();

  fprintf(stdout, "Thread execution finished.\n");
	return(0);
}

void PreprocessTransferNaive(libusb_transfer* transfer, const int read);
void PreprocessTransferFreenect(libusb_transfer* transfer, const int read);
static void(*PreprocessTransfer)(libusb_transfer*, const int) (PreprocessTransferFreenect);

// ReapThreaded Rationale: for each transfer list (stream) of a given device,
// delegate the reap to a dedicated thread for that stream
int ReapThreaded(const libusb_device& dev)
{
  static std::map<const libusb_device*, std::map<int,QuickThread*> > mapDeviceEndPointThreads;

  if (hAbort.Check())
  {
    std::map<int,QuickThread*>& mThreads = mapDeviceEndPointThreads[&dev];
    std::map<int,QuickThread*>::iterator it  (mThreads.begin());
	  std::map<int,QuickThread*>::iterator end (mThreads.end());
    for (; it!=end; ++it)
    {
      QuickThread*& hThread = it->second;
      if (NULL != hThread)
        if (hThread->TryJoin())
          SAFE_DELETE(hThread);
    }

    {
      // On fail guard, release all "ready" transfers as well...
      mutexReady.Enter();
      std::map<libusb_device::TListTransfers*,libusb_device::TListTransfers*>::iterator it  = mapDeviceTransfersReady.begin();
      std::map<libusb_device::TListTransfers*,libusb_device::TListTransfers*>::iterator end = mapDeviceTransfersReady.end();
      for (; it!=end; ++it)
      {
        libusb_device::TListTransfers& lstReady (*(it->second));
        while (!lstReady.Empty())
        {
          transfer_wrapper* wrapper = lstReady.Head();
          libusb_device::TListTransfers::Remove(wrapper);
          libusb_transfer* transfer = &wrapper->libusb;
          transfer->status = LIBUSB_TRANSFER_CANCELLED;
          int read = transfer->actual_length;
          if (read > 0);
            PreprocessTransfer(&wrapper->libusb, wrapper->libusb.actual_length);
          transfer->callback(&wrapper->libusb);
          libusbemu_clear_transfer(wrapper);
        }
      }
      mutexReady.Leave();
    }
    return(-1);
  }

  if (hProblem.Check())
    return(-1);

  libusb_device::TMapIsocTransfers::iterator it  (dev.isoTransfers->begin());
	libusb_device::TMapIsocTransfers::iterator end (dev.isoTransfers->end());
  for (; it!=end; ++it)
  {
    std::map<int,QuickThread*>& mThreads = mapDeviceEndPointThreads[&dev];
    const int endpoint (it->first);
    QuickThread*& hThread = mThreads[endpoint];
    if (NULL != hThread)
    {
      if (hThread->TryJoin())
      {
        SAFE_DELETE(hThread);
        mThreads.erase(endpoint);
      }
    }
    else
    {
      if (!it->second.Empty())
      {
        libusb_device::TListTransfers& listTransfers (it->second);
        hThread = new QuickThread(ReapThreadProc, (void*)&listTransfers);
        hThread->RaisePriority();
      }
    }
  }

  {
  int procs (0);
  libusb_device::TMapIsocTransfers::iterator it  (dev.isoTransfers->begin());
	libusb_device::TMapIsocTransfers::iterator end (dev.isoTransfers->end());
  for (; it!=end; ++it)
  {
    libusb_device::TListTransfers& listTransfers (it->second);
    mutexReady.Enter();
    std::map<libusb_device::TListTransfers*,libusb_device::TListTransfers*>::iterator itReady = mapDeviceTransfersReady.find(&listTransfers);
    if (itReady != mapDeviceTransfersReady.end())
    {
      libusb_device::TListTransfers& listReady = *(itReady->second);
      while (!listReady.Empty())
      {
        ++procs;
        transfer_wrapper* wrapper = listReady.Head();
        libusb_device::TListTransfers::Remove(wrapper);
        libusb_transfer* transfer = &wrapper->libusb;
        int read = transfer->actual_length;
        if (read > 0);
          PreprocessTransfer(&wrapper->libusb, wrapper->libusb.actual_length);
        transfer->callback(&wrapper->libusb);
        libusbemu_clear_transfer(wrapper);
      }
    }
    mutexReady.Leave();
  }
  if (0 == procs)
    QuickThread::Yield();
  }

	return(0);
}

int ReapTransfer(transfer_wrapper* wrapper, int timeout, libusb_device::TListTransfers* lstReady)
{
  void* context (wrapper->usb);
  libusb_transfer* transfer (&wrapper->libusb);

	const int read = usb_reap_async_nocancel(context, timeout);
  if (read >= 0)
  {
    // according to the official libusb_transfer struct reference:
    // "int libusb_transfer::actual_length
    //  Actual length of data that was transferred.
    //  Read-only, and only for use within transfer callback function.
    //  Not valid for isochronous endpoint transfers."
    // since the client will allegedly not read from this field, we'll be using
    // it here just to simplify the emulation implementation, more specifically
    // the libusb_handle_events() and libusbemu_clear_transfer().
    transfer->actual_length = read;

    // are we using the producer/consumer model?
    if (NULL != lstReady)
    {
      libusb_device::TListTransfers::Remove(wrapper);
      mutexReady.Enter();
        lstReady->Append(wrapper);
      mutexReady.Leave();
      return(read);
    }

    // data successfully acquired (0 bytes is also a go!), which means that
    // the transfer should be removed from the head of the list and put into
    // an orphan state; it is up to the client code to resubmit the transfer
    // which will possibly happen during the client callback.
    libusb_device::TListTransfers::Remove(wrapper);

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
    libusbemu_clear_transfer(wrapper);
  }
	else
	{
    switch(read)
    {
      // When usb_reap_async_nocancel() returns ETIMEOUT, then either:
      // (a) the timeout passed to usb_reap_async_nocancel() indeed expired;
      // (b) the transfer was previously cancelled via usb_cancel_async().
      // In case of (a), the transfer must remain as the head of the list
      // (do not remove the node) and just return without calling back (well,
      // it might be a good idea to set the status to LIBUSB_TRANSFER_TIMED_OUT
      // and then callback...) MORE INVESTIGATION NEEDED!
      // In case of (b), the transfer was cancelled and it should be removed
      // from the list and reported through the callback.
      case ETIMEOUT :
        if (LIBUSB_TRANSFER_CANCELLED == transfer->status)
        {
          libusb_device::TListTransfers::Remove(wrapper);
          for (int i=0; i<transfer->num_iso_packets; ++i)
            transfer->iso_packet_desc[i].status = LIBUSB_TRANSFER_CANCELLED;
          transfer->callback(transfer);
          for (int i=0; i<transfer->num_iso_packets; ++i)
            transfer->iso_packet_desc[i].status = LIBUSB_TRANSFER_COMPLETED;
          return(ECANCELLED);
        }
        break;
      case -22 :
        // I guess -22 is returned if one attempts to reap a context that does
        // not exist anymore (one that has already been deleted)
        return(-22);
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
		desc.actual_length = MIN(remaining, desc.length);
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
				desc.actual_length = MIN(remaining, pktlen);
				break;
			case 0x05 : // final
				desc.actual_length = MIN(remaining, pktend);
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
