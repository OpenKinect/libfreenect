/*  libfreenect - an open source Kinect driver

Copyright (C) 2010  Hector Martin "marcan" <hector@marcansoft.com>

This code is licensed to you under the terms of the GNU GPL, version 2 or version 3;
see:
 http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 http://www.gnu.org/licenses/gpl-3.0.txt
*/

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

#endif
