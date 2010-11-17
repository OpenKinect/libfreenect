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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "freenect_internal.h"

int freenect_init(freenect_context **ctx, freenect_usb_context *usb_ctx)
{
	*ctx = malloc(sizeof(freenect_context));
	if (!ctx)
		return -1;

	memset(*ctx, 0, sizeof(freenect_context));

	return fnusb_init(&(*ctx)->usb, usb_ctx);
}

int freenect_shutdown(freenect_context *ctx)
{
	printf("%s NOT IMPLEMENTED YET\n", __FUNCTION__);
	return 0;
}

int freenect_process_events(freenect_context *ctx)
{
	return fnusb_process_events(&ctx->usb);
}

int freenect_num_devices(freenect_context *ctx)
{
	printf("%s NOT IMPLEMENTED YET\n", __FUNCTION__);
	return 0;
}

int freenect_open_device(freenect_context *ctx, freenect_device **dev, int index)
{
	int res;
	freenect_device *pdev = malloc(sizeof(freenect_device));
	if (!pdev)
		return -1;

	memset(pdev, 0, sizeof(*pdev));

	pdev->parent = ctx;

	res = fnusb_open_subdevices(pdev, index);

	if (res < 0) {
		free(pdev);
		return res;
	} else {
		*dev = pdev;
		return 0;
	}
}

int freenect_close_device(freenect_device *dev)
{
	printf("%s NOT IMPLEMENTED YET\n", __FUNCTION__);
	return 0;
}

void freenect_set_user(freenect_device *dev, void *user)
{
	dev->user_data = user;
}
void *freenect_get_user(freenect_device *dev)
{
	return dev->user_data;
}

