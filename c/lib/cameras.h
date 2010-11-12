
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
