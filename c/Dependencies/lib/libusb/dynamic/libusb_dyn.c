/* LIBUSB-WIN32, Generic Windows USB Library
 * Copyright (c) 2002-2005 Stephan Meyer <ste_meyer@web.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include <windows.h>
#include <errno.h>

#include "usb.h"

#define LIBUSB_DLL_NAME "libusb0.dll"


typedef usb_dev_handle * (*usb_open_t)(struct usb_device *dev);
typedef int (*usb_close_t)(usb_dev_handle *dev);
typedef int (*usb_get_string_t)(usb_dev_handle *dev, int index, int langid,
                                char *buf, size_t buflen);
typedef int (*usb_get_string_simple_t)(usb_dev_handle *dev, int index,
                                       char *buf, size_t buflen);
typedef int (*usb_get_descriptor_by_endpoint_t)(usb_dev_handle *udev, int ep,
        unsigned char type,
        unsigned char index,
        void *buf, int size);
typedef int (*usb_get_descriptor_t)(usb_dev_handle *udev, unsigned char type,
                                    unsigned char index, void *buf, int size);
typedef int (*usb_bulk_write_t)(usb_dev_handle *dev, int ep, char *bytes,
                                int size, int timeout);
typedef int (*usb_bulk_read_t)(usb_dev_handle *dev, int ep, char *bytes,
                               int size, int timeout);
typedef int (*usb_interrupt_write_t)(usb_dev_handle *dev, int ep, char *bytes,
                                     int size, int timeout);
typedef int (*usb_interrupt_read_t)(usb_dev_handle *dev, int ep, char *bytes,
                                    int size, int timeout);
typedef int (*usb_control_msg_t)(usb_dev_handle *dev, int requesttype,
                                 int request, int value, int index,
                                 char *bytes, int size, int timeout);
typedef int (*usb_set_configuration_t)(usb_dev_handle *dev, int configuration);
typedef int (*usb_claim_interface_t)(usb_dev_handle *dev, int interface);
typedef int (*usb_release_interface_t)(usb_dev_handle *dev, int interface);
typedef int (*usb_set_altinterface_t)(usb_dev_handle *dev, int alternate);
typedef int (*usb_resetep_t)(usb_dev_handle *dev, unsigned int ep);
typedef int (*usb_clear_halt_t)(usb_dev_handle *dev, unsigned int ep);
typedef int (*usb_reset_t)(usb_dev_handle *dev);
typedef char * (*usb_strerror_t)(void);
typedef void (*usb_init_t)(void);
typedef void (*usb_set_debug_t)(int level);
typedef int (*usb_find_busses_t)(void);
typedef int (*usb_find_devices_t)(void);
typedef struct usb_device * (*usb_device_t)(usb_dev_handle *dev);
typedef struct usb_bus * (*usb_get_busses_t)(void);
typedef int (*usb_install_service_np_t)(void);
typedef int (*usb_uninstall_service_np_t)(void);
typedef int (*usb_install_driver_np_t)(const char *inf_file);
typedef const struct usb_version * (*usb_get_version_t)(void);
typedef int (*usb_isochronous_setup_async_t)(usb_dev_handle *dev,
        void **context,
        unsigned char ep, int pktsize);
typedef int (*usb_bulk_setup_async_t)(usb_dev_handle *dev, void **context,
                                      unsigned char ep);
typedef int (*usb_interrupt_setup_async_t)(usb_dev_handle *dev, void **context,
        unsigned char ep);
typedef int (*usb_submit_async_t)(void *context, char *bytes, int size);
typedef int (*usb_reap_async_t)(void *context, int timeout);
typedef int (*usb_free_async_t)(void **context);


static usb_open_t _usb_open = NULL;
static usb_close_t _usb_close = NULL;
static usb_get_string_t _usb_get_string = NULL;
static usb_get_string_simple_t _usb_get_string_simple = NULL;
static usb_get_descriptor_by_endpoint_t _usb_get_descriptor_by_endpoint = NULL;
static usb_get_descriptor_t _usb_get_descriptor = NULL;
static usb_bulk_write_t _usb_bulk_write = NULL;
static usb_bulk_read_t _usb_bulk_read = NULL;
static usb_interrupt_write_t _usb_interrupt_write = NULL;
static usb_interrupt_read_t _usb_interrupt_read = NULL;
static usb_control_msg_t _usb_control_msg = NULL;
static usb_set_configuration_t _usb_set_configuration = NULL;
static usb_claim_interface_t _usb_claim_interface = NULL;
static usb_release_interface_t _usb_release_interface = NULL;
static usb_set_altinterface_t _usb_set_altinterface = NULL;
static usb_resetep_t _usb_resetep = NULL;
static usb_clear_halt_t _usb_clear_halt = NULL;
static usb_reset_t _usb_reset = NULL;
static usb_strerror_t _usb_strerror = NULL;
static usb_init_t _usb_init = NULL;
static usb_set_debug_t _usb_set_debug = NULL;
static usb_find_busses_t _usb_find_busses = NULL;
static usb_find_devices_t _usb_find_devices = NULL;
static usb_device_t _usb_device = NULL;
static usb_get_busses_t _usb_get_busses = NULL;
static usb_install_service_np_t _usb_install_service_np = NULL;
static usb_uninstall_service_np_t _usb_uninstall_service_np = NULL;
static usb_install_driver_np_t _usb_install_driver_np = NULL;
static usb_get_version_t _usb_get_version = NULL;
static usb_isochronous_setup_async_t _usb_isochronous_setup_async = NULL;
static usb_bulk_setup_async_t _usb_bulk_setup_async = NULL;
static usb_interrupt_setup_async_t _usb_interrupt_setup_async = NULL;
static usb_submit_async_t _usb_submit_async = NULL;
static usb_reap_async_t _usb_reap_async = NULL;
static usb_free_async_t _usb_free_async = NULL;




void usb_init(void)
{
    HINSTANCE libusb_dll  = LoadLibrary(LIBUSB_DLL_NAME);

    if (!libusb_dll)
        return;

    _usb_open = (usb_open_t)
                GetProcAddress(libusb_dll, "usb_open");
    _usb_close = (usb_close_t)
                 GetProcAddress(libusb_dll, "usb_close");
    _usb_get_string = (usb_get_string_t)
                      GetProcAddress(libusb_dll, "usb_get_string");
    _usb_get_string_simple = (usb_get_string_simple_t)
                             GetProcAddress(libusb_dll, "usb_get_string_simple");
    _usb_get_descriptor_by_endpoint = (usb_get_descriptor_by_endpoint_t)
                                      GetProcAddress(libusb_dll, "usb_get_descriptor_by_endpoint");
    _usb_get_descriptor = (usb_get_descriptor_t)
                          GetProcAddress(libusb_dll, "usb_get_descriptor");
    _usb_bulk_write = (usb_bulk_write_t)
                      GetProcAddress(libusb_dll, "usb_bulk_write");
    _usb_bulk_read = (usb_bulk_read_t)
                     GetProcAddress(libusb_dll, "usb_bulk_read");
    _usb_interrupt_write = (usb_interrupt_write_t)
                           GetProcAddress(libusb_dll, "usb_interrupt_write");
    _usb_interrupt_read = (usb_interrupt_read_t)
                          GetProcAddress(libusb_dll, "usb_interrupt_read");
    _usb_control_msg = (usb_control_msg_t)
                       GetProcAddress(libusb_dll, "usb_control_msg");
    _usb_set_configuration = (usb_set_configuration_t)
                             GetProcAddress(libusb_dll, "usb_set_configuration");
    _usb_claim_interface = (usb_claim_interface_t)
                           GetProcAddress(libusb_dll, "usb_claim_interface");
    _usb_release_interface = (usb_release_interface_t)
                             GetProcAddress(libusb_dll, "usb_release_interface");
    _usb_set_altinterface = (usb_set_altinterface_t)
                            GetProcAddress(libusb_dll, "usb_set_altinterface");
    _usb_resetep = (usb_resetep_t)
                   GetProcAddress(libusb_dll, "usb_resetep");
    _usb_clear_halt = (usb_clear_halt_t)
                      GetProcAddress(libusb_dll, "usb_clear_halt");
    _usb_reset = (usb_reset_t)
                 GetProcAddress(libusb_dll, "usb_reset");
    _usb_strerror = (usb_strerror_t)
                    GetProcAddress(libusb_dll, "usb_strerror");
    _usb_init = (usb_init_t)
                GetProcAddress(libusb_dll, "usb_init");
    _usb_set_debug = (usb_set_debug_t)
                     GetProcAddress(libusb_dll, "usb_set_debug");
    _usb_find_busses = (usb_find_busses_t)
                       GetProcAddress(libusb_dll, "usb_find_busses");
    _usb_find_devices = (usb_find_devices_t)
                        GetProcAddress(libusb_dll, "usb_find_devices");
    _usb_device = (usb_device_t)
                  GetProcAddress(libusb_dll, "usb_device");
    _usb_get_busses = (usb_get_busses_t)
                      GetProcAddress(libusb_dll, "usb_get_busses");
    _usb_install_service_np = (usb_install_service_np_t)
                              GetProcAddress(libusb_dll, "usb_install_service_np");
    _usb_uninstall_service_np = (usb_uninstall_service_np_t)
                                GetProcAddress(libusb_dll, "usb_uninstall_service_np");
    _usb_install_driver_np = (usb_install_driver_np_t)
                             GetProcAddress(libusb_dll, "usb_install_driver_np");
    _usb_get_version = (usb_get_version_t)
                       GetProcAddress(libusb_dll, "usb_get_version");
    _usb_isochronous_setup_async = (usb_isochronous_setup_async_t)
                                   GetProcAddress(libusb_dll, "usb_isochronous_setup_async");
    _usb_bulk_setup_async = (usb_bulk_setup_async_t)
                            GetProcAddress(libusb_dll, "usb_bulk_setup_async");
    _usb_interrupt_setup_async = (usb_interrupt_setup_async_t)
                                 GetProcAddress(libusb_dll, "usb_interrupt_setup_async");
    _usb_submit_async = (usb_submit_async_t)
                        GetProcAddress(libusb_dll, "usb_submit_async");
    _usb_reap_async = (usb_reap_async_t)
                      GetProcAddress(libusb_dll, "usb_reap_async");
    _usb_free_async = (usb_free_async_t)
                      GetProcAddress(libusb_dll, "usb_free_async");

    if (_usb_init)
        _usb_init();
}

usb_dev_handle *usb_open(struct usb_device *dev)
{
    if (_usb_open)
        return _usb_open(dev);
    else
        return NULL;
}

int usb_close(usb_dev_handle *dev)
{
    if (_usb_close)
        return _usb_close(dev);
    else
        return -ENOFILE;
}

int usb_get_string(usb_dev_handle *dev, int index, int langid, char *buf,
                   size_t buflen)
{
    if (_usb_get_string)
        return _usb_get_string(dev, index, langid, buf, buflen);
    else
        return -ENOFILE;
}

int usb_get_string_simple(usb_dev_handle *dev, int index, char *buf,
                          size_t buflen)
{
    if (_usb_get_string_simple)
        return _usb_get_string_simple(dev, index, buf, buflen);
    else
        return -ENOFILE;
}

int usb_get_descriptor_by_endpoint(usb_dev_handle *udev, int ep,
                                   unsigned char type, unsigned char index,
                                   void *buf, int size)
{
    if (_usb_get_descriptor_by_endpoint)
        return _usb_get_descriptor_by_endpoint(udev, ep, type, index, buf, size);
    else
        return -ENOFILE;
}

int usb_get_descriptor(usb_dev_handle *udev, unsigned char type,
                       unsigned char index, void *buf, int size)
{
    if (_usb_get_descriptor)
        return _usb_get_descriptor(udev, type, index, buf, size);
    else
        return -ENOFILE;
}

int usb_bulk_write(usb_dev_handle *dev, int ep, char *bytes, int size,
                   int timeout)
{
    if (_usb_bulk_write)
        return _usb_bulk_write(dev, ep, bytes, size, timeout);
    else
        return -ENOFILE;
}

int usb_bulk_read(usb_dev_handle *dev, int ep, char *bytes, int size,
                  int timeout)
{
    if (_usb_bulk_read)
        return _usb_bulk_read(dev, ep, bytes, size, timeout);
    else
        return -ENOFILE;
}

int usb_interrupt_write(usb_dev_handle *dev, int ep, char *bytes, int size,
                        int timeout)
{
    if (_usb_interrupt_write)
        return _usb_interrupt_write(dev, ep, bytes, size, timeout);
    else
        return -ENOFILE;
}

int usb_interrupt_read(usb_dev_handle *dev, int ep, char *bytes, int size,
                       int timeout)
{
    if (_usb_interrupt_read)
        return _usb_interrupt_read(dev, ep, bytes, size, timeout);
    else
        return -ENOFILE;
}

int usb_control_msg(usb_dev_handle *dev, int requesttype, int request,
                    int value, int index, char *bytes, int size,
                    int timeout)
{
    if (_usb_control_msg)
        return _usb_control_msg(dev, requesttype, request, value, index, bytes,
                                size, timeout);
    else
        return -ENOFILE;
}

int usb_set_configuration(usb_dev_handle *dev, int configuration)
{
    if (_usb_set_configuration)
        return _usb_set_configuration(dev, configuration);
    else
        return -ENOFILE;
}

int usb_claim_interface(usb_dev_handle *dev, int interface)
{
    if (_usb_claim_interface)
        return _usb_claim_interface(dev, interface);
    else
        return -ENOFILE;
}

int usb_release_interface(usb_dev_handle *dev, int interface)
{
    if (_usb_release_interface)
        return _usb_release_interface(dev, interface);
    else
        return -ENOFILE;
}

int usb_set_altinterface(usb_dev_handle *dev, int alternate)
{
    if (_usb_set_altinterface)
        return _usb_set_altinterface(dev, alternate);
    else
        return -ENOFILE;
}

int usb_resetep(usb_dev_handle *dev, unsigned int ep)
{
    if (_usb_resetep)
        return _usb_resetep(dev, ep);
    else
        return -ENOFILE;
}

int usb_clear_halt(usb_dev_handle *dev, unsigned int ep)
{
    if (_usb_clear_halt)
        return _usb_clear_halt(dev, ep);
    else
        return -ENOFILE;
}

int usb_reset(usb_dev_handle *dev)
{
    if (_usb_reset)
        return _usb_reset(dev);
    else
        return -ENOFILE;
}

char *usb_strerror(void)
{
    if (_usb_strerror)
        return _usb_strerror();
    else
        return NULL;
}

void usb_set_debug(int level)
{
    if (_usb_set_debug)
        return _usb_set_debug(level);
}

int usb_find_busses(void)
{
    if (_usb_find_busses)
        return _usb_find_busses();
    else
        return -ENOFILE;
}

int usb_find_devices(void)
{
    if (_usb_find_devices)
        return _usb_find_devices();
    else
        return -ENOFILE;
}

struct usb_device *usb_device(usb_dev_handle *dev)
{
    if (_usb_device)
        return _usb_device(dev);
    else
        return NULL;
}

struct usb_bus *usb_get_busses(void)
{
    if (_usb_get_busses)
        return _usb_get_busses();
    else
        return NULL;
}

int usb_install_service_np(void)
{
    if (_usb_install_service_np)
        return _usb_install_service_np();
    else
        return -ENOFILE;
}

int usb_uninstall_service_np(void)
{
    if (_usb_uninstall_service_np)
        return _usb_uninstall_service_np();
    else
        return -ENOFILE;
}

int usb_install_driver_np(const char *inf_file)
{
    if (_usb_install_driver_np)
        return _usb_install_driver_np(inf_file);
    else
        return -ENOFILE;
}

const struct usb_version *usb_get_version(void)
{
    if (_usb_get_version)
        return _usb_get_version();
    else
        return NULL;
}

int usb_isochronous_setup_async(usb_dev_handle *dev, void **context,
                                unsigned char ep, int pktsize)
{
    if (_usb_isochronous_setup_async)
        return _usb_isochronous_setup_async(dev, context, ep, pktsize);
    else
        return -ENOFILE;
}

int usb_bulk_setup_async(usb_dev_handle *dev, void **context,
                         unsigned char ep)
{
    if (_usb_bulk_setup_async)
        return _usb_bulk_setup_async(dev, context, ep);
    else
        return -ENOFILE;
}

int usb_interrupt_setup_async(usb_dev_handle *dev, void **context,
                              unsigned char ep)
{
    if (_usb_interrupt_setup_async)
        return _usb_interrupt_setup_async(dev, context, ep);
    else
        return -ENOFILE;
}

int usb_submit_async(void *context, char *bytes, int size)
{
    if (_usb_submit_async)
        return _usb_submit_async(context, bytes, size);
    else
        return -ENOFILE;
}

int usb_reap_async(void *context, int timeout)
{
    if (_usb_reap_async)
        return _usb_reap_async(context, timeout);
    else
        return -ENOFILE;
}

int usb_free_async(void **context)
{
    if (_usb_free_async)
        return _usb_free_async(context);
    else
        return -ENOFILE;
}
