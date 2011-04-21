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

#include <string.h>

#include "freenect_internal.h"

typedef struct {
	uint8_t magic[2];
	uint16_t len;
	uint16_t cmd;
	uint16_t tag;
} control_header;

int send_cmd(freenect_device *dev, uint16_t cmd, void *cmd_buf, unsigned int cmd_buf_size, void *replybuf, unsigned int reply_len)
{
	freenect_context *ctx = dev->parent;
	int res, actual_len;
	uint8_t obuf[0x400];
	uint8_t ibuf[0x200];

	control_header *chdr = (control_header*)obuf;
	control_header *rhdr = (control_header*)ibuf;

	if (cmd_buf_size & 1 || cmd_buf_size > (0x400 - sizeof(*chdr))) {
		FN_ERROR("send_cmd: Invalid command length (0x%x)\n", cmd_buf_size);
		return -1;
	}

	chdr->magic[0] = 0x47;
	chdr->magic[1] = 0x4d;
	chdr->cmd = fn_le16(cmd);
	chdr->tag = fn_le16(dev->cam_tag);
	chdr->len = fn_le16(cmd_buf_size / 2);

	memcpy(obuf+sizeof(*chdr), cmd_buf, cmd_buf_size);

	res = fnusb_control(&dev->usb_cam, 0x40, 0, 0, 0, obuf, cmd_buf_size + sizeof(*chdr));
	FN_SPEW("Control cmd=%04x tag=%04x len=%04x: %d\n", cmd, dev->cam_tag, cmd_buf_size, res);
	if (res < 0) {
		FN_ERROR("send_cmd: Output control transfer failed (%d)\n", res);
		return res;
	}

	do {
		actual_len = fnusb_control(&dev->usb_cam, 0xc0, 0, 0, 0, ibuf, 0x200);
	} while (actual_len == 0);
	FN_SPEW("Control reply: %d\n", res);
	if (actual_len < sizeof(*rhdr)) {
		FN_ERROR("send_cmd: Input control transfer failed (%d)\n", res);
		return res;
	}
	actual_len -= sizeof(*rhdr);

	if (rhdr->magic[0] != 0x52 || rhdr->magic[1] != 0x42) {
		FN_ERROR("send_cmd: Bad magic %02x %02x\n", rhdr->magic[0], rhdr->magic[1]);
		return -1;
	}
	if (rhdr->cmd != chdr->cmd) {
		FN_ERROR("send_cmd: Bad cmd %02x != %02x\n", rhdr->cmd, chdr->cmd);
		return -1;
	}
	if (rhdr->tag != chdr->tag) {
		FN_ERROR("send_cmd: Bad tag %04x != %04x\n", rhdr->tag, chdr->tag);
		return -1;
	}
	if (fn_le16(rhdr->len) != (actual_len/2)) {
		FN_ERROR("send_cmd: Bad len %04x != %04x\n", fn_le16(rhdr->len), (int)(actual_len/2));
		return -1;
	}

	if (actual_len > reply_len) {
		FN_WARNING("send_cmd: Data buffer is %d bytes long, but got %d bytes\n", reply_len, actual_len);
		memcpy(replybuf, ibuf+sizeof(*rhdr), reply_len);
	} else {
		memcpy(replybuf, ibuf+sizeof(*rhdr), actual_len);
	}

	dev->cam_tag++;

	return actual_len;
}
