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
#include <unistd.h>
#include <string.h>
#include "libfreenect_sync.h"
#include <pthread.h>
#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <arpa/inet.h>
#endif
#include <signal.h>


#include <math.h>

int g_argc;
char **g_argv;

#define AS3_BITMAPDATA_LEN FREENECT_FRAME_PIX * 4

#ifdef WIN32
struct addrinfo si_depth, si_rgb, si_data;
#else
struct sockaddr_in si_depth, si_rgb, si_data;
#endif

pthread_t depth_thread, rgb_thread, data_thread, data_in_thread, data_out_thread, rgb_out_thread, depth_out_thread;
int freenect_angle = 15;

#ifdef WIN32
PCSTR conf_port_depth = "6001",
conf_port_rgb	= "6002",
conf_port_data	= "6003";

SOCKET depth_socket = INVALID_SOCKET,
depth_client_socket = INVALID_SOCKET,
rgb_socket = INVALID_SOCKET,
rgb_client_socket = INVALID_SOCKET,
data_socket = INVALID_SOCKET,
data_client_socket = INVALID_SOCKET;
#else
char *conf_ip		= "127.0.0.1";
int s_depth			= -1,
s_rgb			= -1,
s_data			= -1,
conf_port_depth	= 6001,
conf_port_rgb	= 6002,
conf_port_data	= 6003;
#endif

uint8_t buf_depth[AS3_BITMAPDATA_LEN];
uint8_t	buf_rgb[AS3_BITMAPDATA_LEN];

int die = 0;

int depth_child;
int depth_connected = 0;

int rgb_child;
int rgb_connected = 0;

int data_child;
int data_connected = 0;

int psent = 0;

#ifdef WIN32
int initServer(addrinfo si_type, PCSTR conf_port, SOCKET *the_socket, PCSTR label){
	ZeroMemory(&si_type, sizeof (si_type));

	si_type.ai_family = AF_INET;
	si_type.ai_socktype = SOCK_STREAM;
	si_type.ai_protocol = IPPROTO_TCP;
	si_type.ai_flags = AI_PASSIVE;
	
	// Resolve the local address and port to be used by the server
	struct addrinfo *result = NULL;	

	int iResult = getaddrinfo(NULL, conf_port, &si_type, &result);
	if (iResult != 0) {
		printf("%s: getaddrinfo failed: %d\n", label, iResult);
		WSACleanup();
		return 1;
	}

	*the_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);

	if (*the_socket == INVALID_SOCKET) {
		printf("%s: socket failed [%ld]\n", label, WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return 1;
	}

	iResult = bind(*the_socket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		printf("%s: bind failed: %d\n", label, WSAGetLastError());
		freeaddrinfo(result);
		closesocket(*the_socket);
		WSACleanup();
		return 1;
	}

	freeaddrinfo(result);

	if ( listen(*the_socket, SOMAXCONN ) == SOCKET_ERROR ) {
		printf( "%s: listen failed [%ld]\n", label, WSAGetLastError() );
		closesocket(*the_socket);
		WSACleanup();
		return 1;
	}

	return 0;
}
#else 
int initServer(struct sockaddr_in si_type, int conf_port, int *the_socket) {
	int optval = 1;
	if ( (*the_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1 )
	{
		fprintf(stderr, "Unable to create depth socket\n");
		return -1;
	}

	setsockopt(*the_socket, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(optval));

	memset((char *) &si_type, 0, sizeof(si_type));

	si_type.sin_family			= AF_INET;
	si_type.sin_port			= htons(conf_port);
	si_type.sin_addr.s_addr		= inet_addr(conf_ip);

	if ( bind(*the_socket, (struct sockaddr *)&si_type,
			  sizeof(si_type)) < 0 )
	{
		fprintf(stderr, "Error at bind() for depth\n");
		return -1;
	}
	if ( listen(*the_socket, 1) < 0 )
	{
		fprintf(stderr, "Error on listen() for depth\n");
		return -1;
	}

	return 0;
}
#endif

void *data_in(void *arg) {
	int n;
	while(data_connected){
		char buffer[6];
		#ifdef WIN32
		n = recv(data_client_socket, (char*)buffer, 1024, 0);
		#else
		n = read(data_child, buffer, 1024);
		#endif
		if(n == 6){
			if (buffer[0] == 1) { //MOTOR
				if (buffer[1] == 1) { //MOVE
					int angle;
					memcpy(&angle, &buffer[2], sizeof(int));
					freenect_sync_set_tilt_degs(ntohl(angle), 0);
				}
			}
		}
	}
	return NULL;
}

void *data_out(void *arg) {
	int n;
	int16_t ax,ay,az;
	double dx,dy,dz;
	while(data_connected){
		#ifdef WIN32
		Sleep(1000/30);
		#else
		usleep(1000000 / 30); // EMULATE 30 FPS
		#endif
		char buffer_send[3*2+3*8];
		freenect_raw_tilt_state *state;
		freenect_sync_get_tilt_state(&state, 0);
		ax = state->accelerometer_x;
		ay = state->accelerometer_y;
		az = state->accelerometer_z;
		freenect_get_mks_accel(state, &dx, &dy, &dz);
		memcpy(&buffer_send,&ax, sizeof(int16_t));
		memcpy(&buffer_send[2],&ay, sizeof(int16_t));
		memcpy(&buffer_send[4],&az, sizeof(int16_t));
		memcpy(&buffer_send[6],&dx, sizeof(double));
		memcpy(&buffer_send[14],&dy, sizeof(double));
		memcpy(&buffer_send[22],&dz, sizeof(double));
		#ifdef WIN32
		int n = send(data_client_socket, (char*)buffer_send, 3*2+3*8, 0);
		#else
		n = write(data_child, buffer_send, 3*2+3*8);
		#endif
	}
	return NULL;
}

void *depth_out(void *arg){
	while ( depth_connected )
	{
		uint32_t ts,i;
		void *buf_depth_temp;
		freenect_sync_get_depth(&buf_depth_temp, &ts, 0, FREENECT_DEPTH_11BIT);
		uint16_t *depth = (uint16_t*)buf_depth_temp;

		for (i=0; i<FREENECT_FRAME_PIX; i++) {
			buf_depth[4 * i + 0] = 0x00;
			buf_depth[4 * i + 1] = 0x00;
			buf_depth[4 * i + 2] = 0x00;
			buf_depth[4 * i + 3] = 0xFF;
			if(depth[i] < 800 && depth[i] > 600){
				buf_depth[4 * i + 0] = 0xFF;
				buf_depth[4 * i + 1] = 0xFF;
				buf_depth[4 * i + 2] = 0xFF;
				buf_depth[4 * i + 3] = 0xFF;
			}
		}
		#ifdef WIN32
		int n = send(depth_client_socket, (char*)buf_depth, AS3_BITMAPDATA_LEN, 0);
		#else
		int n = write(depth_child, &buf_depth, AS3_BITMAPDATA_LEN);
		#endif
		if ( n < 0 || n != AS3_BITMAPDATA_LEN)
		{
			//fprintf(stderr, "Error on write() for depth (%d instead of %d)\n",n, AS3_BITMAPDATA_LEN);
			depth_connected = 0;
		}
	}
	return 0;
}

void *rgb_out(void *arg){
	while (rgb_connected)
	{
		uint32_t ts,i;
		void *buf_rgb_temp;
		freenect_sync_get_video(&buf_rgb_temp, &ts, 0, FREENECT_VIDEO_RGB);
		uint8_t *rgb = (uint8_t*)buf_rgb_temp;
		for (i=0; i<FREENECT_FRAME_PIX; i++) {
			buf_rgb[4 * i + 0] = rgb[3 * i + 2];
			buf_rgb[4 * i + 1] = rgb[3 * i + 1];
			buf_rgb[4 * i + 2] = rgb[3 * i + 0];
			buf_rgb[4 * i + 3] = 0x00;
		}
		#ifdef WIN32
		int n = send(rgb_client_socket, (char*)buf_rgb, AS3_BITMAPDATA_LEN, 0);
		#else
		int n = write(rgb_child, &buf_rgb, AS3_BITMAPDATA_LEN);
		#endif
		
		if ( n < 0 || n != AS3_BITMAPDATA_LEN)
		{
			//fprintf(stderr, "Error on write() for rgb (%d instead of %d)\n",n, AS3_BITMAPDATA_LEN);
			rgb_connected = 0;
		}
	}
	return NULL;
}

void send_policy_file(int child){
	if(psent == 0){
		char * str = "<?xml version='1.0'?><!DOCTYPE cross-domain-policy SYSTEM '/xml/dtds/cross-domain-policy.dtd'><cross-domain-policy><site-control permitted-cross-domain-policies='all'/><allow-access-from domain='*' to-ports='*'/></cross-domain-policy>\n";
		#ifdef WIN32
		int n = send(data_client_socket, (char*)str, 235, 0);
		#else
		int n = write(child,str , 235);
		#endif
		
		if ( n < 0 || n != 235)
		{
			fprintf(stderr, "Error on write() for policy (%d instead of %d)\n",n, 235);
		}
		//psent = 1;
	}
}

void *network_depth(void *arg)
{
	int childlen;
	struct sockaddr_in childaddr;
	
	childlen = sizeof(childaddr);
	while ( !die )
	{
		printf("### Wait depth client\n");
		#ifdef WIN32
		depth_client_socket = accept(depth_socket, NULL, NULL);
		if (depth_client_socket == INVALID_SOCKET) {
			printf("Error on accept() for depth, exit depth thread. %d\n", WSAGetLastError());
			closesocket(depth_socket);
			WSACleanup();
			break;
		}
		#else
		depth_child = accept(s_depth, (struct sockaddr *)&childaddr, (unsigned int *)&childlen);
		if ( network_depth < 0 )
		{
			fprintf(stderr, "Error on accept() for depth, exit depth thread.\n");
			break;
		}
		#endif
		
		printf("### Got depth client\n");
		send_policy_file(depth_child);
		depth_connected = 1;
		if ( pthread_create(&depth_out_thread, NULL, depth_out, NULL) )
		{
			fprintf(stderr, "Error on pthread_create() for depth_out\n");
		}
	}
	
	return NULL;
}

void *network_rgb(void *arg)
{
	int childlen;
	struct sockaddr_in childaddr;
	
	childlen = sizeof(childaddr);
	while ( !die )
	{
		printf("### Wait rgb client\n");
		#ifdef WIN32
		rgb_client_socket = accept(rgb_socket, NULL, NULL);
		if (rgb_client_socket == INVALID_SOCKET) {
			printf("Error on accept() for rgb, exit rgb thread. %d\n", WSAGetLastError());
			closesocket(rgb_socket);
			WSACleanup();
			break;
		}
		#else
		rgb_child = accept(s_rgb, (struct sockaddr *)&childaddr, (unsigned int *)&childlen);
		if ( rgb_child < 0 )
		{
			fprintf(stderr, "Error on accept() for rgb, exit rgb thread.\n");
			break;
		}
		#endif
		
		printf("### Got rgb client\n");
		send_policy_file(rgb_child);
		rgb_connected = 1;
		if ( pthread_create(&rgb_out_thread, NULL, rgb_out, NULL) )
		{
			fprintf(stderr, "Error on pthread_create() for rgb_out\n");
		}
	}
	
	return NULL;
}

void *network_data(void *arg)
{
	int childlen;
	struct sockaddr_in childaddr;
	
	childlen = sizeof(childaddr);
	while ( !die )
	{
		printf("### Wait data client\n");
		#ifdef WIN32
		data_client_socket = accept(data_socket, NULL, NULL);
		if (data_client_socket == INVALID_SOCKET) {
			printf("Error on accept() for data, exit data thread. %d\n", WSAGetLastError());
			closesocket(data_socket);
			WSACleanup();
			break;
		}
		#else
		data_child = accept(s_data, (struct sockaddr *)&childaddr, (unsigned int *)&childlen);
		if ( data_child < 0 )
		{
			fprintf(stderr, "Error on accept() for data, exit data thread.\n");
			break;
		}
		#endif
		
		printf("### Got data client\n");
		data_connected = 1;
		
		send_policy_file(data_child);
		
		if ( pthread_create(&data_in_thread, NULL, data_in, NULL) )
		{
			fprintf(stderr, "Error on pthread_create() for data_in\n");
		}
		
		if ( pthread_create(&data_out_thread, NULL, data_out, NULL) )
		{
			fprintf(stderr, "Error on pthread_create() for data_out\n");
		}
	}
	
	return NULL;
}

int network_init()
{
	#ifndef WIN32
	signal(SIGPIPE, SIG_IGN);
	initServer(si_depth, conf_port_depth, &s_depth);
	initServer(si_rgb, conf_port_rgb, &s_rgb);
	initServer(si_data, conf_port_data, &s_data);
	#else
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;

	/* Use the MAKEWORD(lowbyte, highbyte) macro declared in Windef.h */
	wVersionRequested = MAKEWORD(2, 2);

	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		/* Tell the user that we could not find a usable */
		/* Winsock DLL.                                  */
		printf("WSAStartup failed with error: %d\n", err);
		return 1;
	}
	initServer(si_depth, conf_port_depth, &depth_socket, "DEPTH");
	initServer(si_rgb, conf_port_rgb, &rgb_socket, "RGB");
	initServer(si_data, conf_port_data, &data_socket, "DATA");
	#endif

	/* launch 3 threads, 2 for each images and 1 for control
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
	
	if ( pthread_create(&data_thread, NULL, network_data, NULL) )
	{
		fprintf(stderr, "Error on pthread_create() for data\n");
		return -1;
	}

	return 0;
}

void network_close()
{
	die = 1;
#ifdef WIN32
	if ( depth_socket != INVALID_SOCKET )
		closesocket(depth_socket);
	if ( rgb_socket != INVALID_SOCKET )
		closesocket(rgb_socket);
	if ( data_socket != INVALID_SOCKET )
		closesocket(data_socket);
#else
	if ( s_depth != -1 )
		close(s_depth), s_depth = -1;
	if ( s_rgb != -1 )
		close(s_rgb), s_rgb = -1;
	if ( s_data != -1 )
		close(s_data), s_data = -1;
#endif
}

int main(int argc, char **argv)
{
	if ( network_init() < 0 )
		return -1;
	
	while(!die);

	return 0;
}
