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

#ifndef LIBUSB_EMULATOR_INTERNAL_H
#define LIBUSB_EMULATOR_INTERNAL_H

#include "libusbemu_threads.h"
#include <usb.h>
#include <map>
#include <cassert>

namespace libusbemu {

template<typename T>
inline T*& SAFE_DELETE(T*& p)
{
  if(NULL != p)
    delete(p);
  p = NULL;
  return(p);
}

template<typename T>
inline T MIN(const T& v1, const T& v2)
{
  return((v1<v2) ? v1 : v2);
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

} // end of 'namespace libusbemu'



using namespace libusbemu;

struct transfer_wrapper
{
	transfer_wrapper* prev;
	transfer_wrapper* next;
	void* usb;
	libusb_transfer libusb;
};

struct libusb_device_handle_t
{
	libusb_device* dev;
  usb_dev_handle* handle;
};

struct libusb_device_t
{
	libusb_context* ctx;
  struct usb_device* device;
  int refcount;
  typedef QuickList<transfer_wrapper> TListTransfers;
  typedef std::map<int, TListTransfers> TMapIsocTransfers;
  TMapIsocTransfers* isoTransfers;
  typedef std::map<usb_dev_handle*, libusb_device_handle> TMapHandles;
  TMapHandles* handles;
};

struct libusb_context_t
{
  typedef std::map<struct usb_device*, libusb_device> TMapDevices;
  TMapDevices devices;
  QuickMutex mutex;
  QuickEvent processing;
};



namespace libusbemu {

transfer_wrapper* libusbemu_get_transfer_wrapper(libusb_transfer* transfer)
{
  char* raw_address ((char*)transfer);
  char* off_address (raw_address - sizeof(void*) - 2*sizeof(transfer_wrapper*));
  return((transfer_wrapper*)off_address);
}

libusb_device* libusbemu_register_device(libusb_context* ctx, struct usb_device* dev)
{
  // register the device (if not already there) ...
  libusb_device dummy = { ctx, dev, 0, NULL, NULL };
  libusb_context::TMapDevices::iterator it = ctx->devices.insert(std::make_pair(dev,dummy)).first;
  // ... and increment the reference count
  libusb_device& record (it->second);
  record.refcount++;
  // might as well do some paranoid checkings...
  assert(record.ctx == ctx);
  assert(record.device == dev);
  return(&(it->second));
}

void libusbemu_unregister_device(libusb_device* dev)
{
  // decrement the reference count of the device ...
  --(dev->refcount);
  // ... and unregister device if the reference count reaches zero
  if (0 == dev->refcount)
  {
    SAFE_DELETE(dev->handles);
    // prior to device deletion, all of its transfer lists must be deleted
    if (NULL != dev->isoTransfers)
    {
      libusb_device::TMapIsocTransfers& allTransfers (*(dev->isoTransfers));
      while (!allTransfers.empty())
      {
        libusb_device::TMapIsocTransfers::iterator it (allTransfers.begin());
        libusb_device::TListTransfers& listTransfers (it->second);
        while (!listTransfers.Empty())
        {
          transfer_wrapper* transfer (listTransfers.Head());
          // make it orphan so that it can be deleted:
          libusb_device::TListTransfers::Remove(transfer);
          // the following will free the wrapper object as well:
          libusb_free_transfer(&transfer->libusb);
        }
        allTransfers.erase(it);
      }
      SAFE_DELETE(dev->isoTransfers);
    }
    libusb_context* ctx (dev->ctx);
    ctx->devices.erase(dev->device);
  }
}

int libusbemu_setup_transfer(transfer_wrapper* wrapper)
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

void libusbemu_clear_transfer(transfer_wrapper* wrapper)
{
  libusb_transfer* transfer (&wrapper->libusb);
  if (transfer->actual_length > 0)
  {
    transfer->actual_length = 0;
    memset(transfer->buffer, 0, transfer->length);
    for (int i=0; i<transfer->num_iso_packets; ++i)
      transfer->iso_packet_desc[i].actual_length = 0;
  }
}

} // end of 'namespace libusbemu'

#endif//LIBUSB_EMULATOR_INTERNAL_H
