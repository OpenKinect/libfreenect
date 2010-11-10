#ifndef CAMERAS_H
#define CAMERAS_H

#include <stdint.h>

struct caminit {
	uint16_t command;
	uint16_t tag;
	int cmdlen;
	int replylen;
	uint8_t cmddata[1024];
	uint8_t replydata[1024];
};

typedef void (*depthcb)(uint16_t *buf, int width, int height);
typedef void (*rgbcb)(uint8_t *buf, int width, int height);

void cams_init(libusb_device_handle *d, depthcb depth_cb, rgbcb rgb_cb);

#endif
