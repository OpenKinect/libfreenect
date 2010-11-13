/* kinect-network - send rgb and depth image over network
 *
 * Copyright (C) 2010  Mathieu Virbel "tito" <mathieu@pymt.eu>
 *
 * This code is licensed to you under the terms of the GNU GPL, version 2 or version 3.
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 * http://www.gnu.org/licenses/gpl-3.0.txt
 *
 * Depth image is send over TCP port 6001. Size is 640 * 480 * sizeof(uint16_t)
 * RGB image is send over TCP port 6002. Size is 640 * 480 * 3
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <libusb.h>
#include "libfreenect.h"

#include <pthread.h>
#include <arpa/inet.h>
#include <signal.h>

#define DEPTH_LEN		(640 * 480)
#define DEPTH_SZ		DEPTH_LEN * sizeof(uint16_t)
#define RGB_LEN			(640 * 480 * 3)
#define RGB_SZ			RGB_LEN * sizeof(uint8_t)

struct sockaddr_in si_depth, si_rgb;
pthread_t depth_thread, rgb_thread;
pthread_mutex_t depth_mutex	= PTHREAD_MUTEX_INITIALIZER,
			    rgb_mutex	= PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t depth_cond	= PTHREAD_COND_INITIALIZER,
			   rgb_cond		= PTHREAD_COND_INITIALIZER;
char *conf_ip		= "127.0.0.1";
int s_depth			= -1,
	s_rgb			= -1,
	conf_port_depth	= 6001,
	conf_port_rgb	= 6002;
uint16_t buf_depth[DEPTH_LEN];
uint8_t	buf_rgb[RGB_LEN];
int die = 0;

void *network_depth(void *arg)
{
	int child, childlen, n;
	struct sockaddr_in childaddr;
	uint16_t buf[DEPTH_LEN];

	childlen = sizeof(childaddr);
	while ( !die )
	{
		printf("### Wait depth client\n");
		child = accept(s_depth, (struct sockaddr *)&childaddr,
				(unsigned int *)&childlen);
		if ( child < 0 )
		{
			fprintf(stderr, "Error on accept() for depth, exit depth thread.\n");
			break;
		}

		printf("### Got depth client\n");
		while ( !die )
		{
			pthread_mutex_lock(&depth_mutex);
			pthread_cond_wait(&depth_cond, &depth_mutex);
			memcpy(buf, buf_depth, DEPTH_LEN);
			pthread_mutex_unlock(&depth_mutex);

			n = write(child, buf_depth, DEPTH_SZ);

			if ( n < 0 || n != DEPTH_SZ )
			{
				fprintf(stderr, "Error on write() for depth (%d instead of %ld)\n",
						n, DEPTH_SZ);
				break;
			}
		}
	}

	return NULL;
}

void *network_rgb(void *arg)
{
	int child, childlen, n;
	struct sockaddr_in childaddr;
	uint8_t buf[RGB_LEN];

	childlen = sizeof(childaddr);
	while ( !die )
	{
		printf("### Wait rgb client\n");
		child = accept(s_rgb, (struct sockaddr *)&childaddr,
				(unsigned int *)&childlen);
		if ( child < 0 )
		{
			fprintf(stderr, "Error on accept() for rgb, exit rgb thread.\n");
			break;
		}

		printf("### Got rgb client\n");
		while ( !die )
		{
			pthread_mutex_lock(&depth_mutex);
			pthread_cond_wait(&depth_cond, &depth_mutex);
			memcpy(buf, buf_depth, RGB_LEN);
			pthread_mutex_unlock(&depth_mutex);

			n = write(child, buf_rgb, RGB_SZ);

			if ( n < 0 || n != RGB_SZ )
			{
				fprintf(stderr, "Error on write() for rgb (%d instead of %ld)\n",
						n, RGB_SZ);
				break;
			}
		}
	}

	return NULL;
}

int network_init()
{
	int optval = 1;

	signal(SIGPIPE, SIG_IGN);

	if ( (s_depth = socket(AF_INET, SOCK_STREAM, 0)) == -1 )
	{
		fprintf(stderr, "Unable to create depth socket\n");
		return -1;
	}

	if ( (s_rgb = socket(AF_INET, SOCK_STREAM, 0)) == -1 )
	{
		fprintf(stderr, "Unable to create rgb socket\n");
		return -1;
	}

	setsockopt(s_depth, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(optval));
	setsockopt(s_rgb, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(optval));

	memset((char *) &si_depth, 0, sizeof(si_depth));
	memset((char *) &si_rgb, 0, sizeof(si_rgb));

	si_depth.sin_family			= AF_INET;
	si_depth.sin_port			= htons(conf_port_depth);
	si_depth.sin_addr.s_addr	= inet_addr(conf_ip);

	si_rgb.sin_family			= AF_INET;
	si_rgb.sin_port				= htons(conf_port_rgb);
	si_rgb.sin_addr.s_addr		= inet_addr(conf_ip);

	if ( bind(s_depth, (struct sockaddr *)&si_depth,
				sizeof(si_depth)) < 0 )
	{
		fprintf(stderr, "Error at bind() for depth\n");
		return -1;
	}

	if ( bind(s_rgb, (struct sockaddr *)&si_rgb,
				sizeof(si_rgb)) < 0 )
	{
		fprintf(stderr, "Error at bind() for rgb\n");
		return -1;
	}

	if ( listen(s_depth, 1) < 0 )
	{
		fprintf(stderr, "Error on listen() for depth\n");
		return -1;
	}

	if ( listen(s_rgb, 1) < 0 )
	{
		fprintf(stderr, "Error on listen() for rgb\n");
		return -1;
	}

	/* launch 2 thread for each images
	 */

	if ( pthread_create(&depth_thread, NULL, network_depth, NULL) )
	{
		fprintf(stderr, "Error on pthread_create() for depth\n");
		return -1;
	}

	if ( pthread_create(&rgb_thread, NULL, network_rgb, NULL) )
	{
		fprintf(stderr, "Error on pthread_create() for rgb\n");
		return -1;
	}

	return 0;
}

void network_close()
{
	die = 1;
	if ( s_depth != -1 )
		close(s_depth), s_depth = -1;
	if ( s_rgb != -1 )
		close(s_rgb), s_rgb = -1;
}

void depthimg(uint16_t *buf, int width, int height)
{
	pthread_mutex_lock(&depth_mutex);
	memcpy(buf_depth, buf, DEPTH_SZ);
	pthread_cond_signal(&depth_cond);
	pthread_mutex_unlock(&depth_mutex);
}

void rgbimg(uint8_t *buf, int width, int height)
{
	pthread_mutex_lock(&depth_mutex);
	memcpy(buf_rgb, buf, RGB_SZ);
	pthread_cond_signal(&depth_cond);
	pthread_mutex_unlock(&depth_mutex);
}

int main(int argc, char **argv)
{
	libusb_device_handle *dev;

	printf("Kinect camera - send raw image on network\n");

	if ( network_init() < 0 )
		return -1;

	libusb_init(NULL);

	dev = libusb_open_device_with_vid_pid(NULL, 0x45e, 0x2ae);
	if (!dev)
	{
		printf("Could not open device\n");
		return 1;
	}

	libusb_claim_interface(dev, 0);

	cams_init(dev, depthimg, rgbimg);

	while (libusb_handle_events(NULL) == 0);

	network_close();

	return 0;
}
