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
#ifndef _WIN32
	#include <unistd.h>
	#include <arpa/inet.h>
#else
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#pragma comment(lib, "Ws2_32.lib")
	#include <stdint.h>
	#include <windows.h>
	unsigned sleep(unsigned seconds)
	{
	  Sleep(seconds*1000);  // The Windows Sleep operates on milliseconds
	  return(0);
	}
#endif

#include <string.h>
#include "libfreenect_sync.h"
#include <pthread.h>
#include <signal.h>
#include <math.h>

int g_argc;
char **g_argv;

char *_current_version = "v9.0b";

#define AS3_BITMAPDATA_LEN FREENECT_FRAME_PIX * 4

pthread_t wait_for_client_thread, connection_thread;

#ifdef WIN32
	struct addrinfo si_data;
	PCSTR conf_port_data	= "6001";
	SOCKET data_socket = INVALID_SOCKET,
		data_client_socket = INVALID_SOCKET;
#else
	struct sockaddr_in si_data;
	char *conf_ip		= "127.0.0.1";
	int s_data			= -1,
		conf_port_data	= 6001;
#endif

uint8_t buf_depth[AS3_BITMAPDATA_LEN];
uint8_t	buf_rgb[AS3_BITMAPDATA_LEN];
void *buf_depth_temp;
void *buf_rgb_temp;

int die = 0;

int depth_child;
int depth_connected = 0;

int rgb_child;
int rgb_connected = 0;

int data_child;
int client_connected = 0;

int psent = 0;

int _video_mirrored = 0,
	_depth_mirrored = 0,
	_min_depth = 600,
	_max_depth = 800;

#ifdef WIN32
int initServer(addrinfo si_type, PCSTR conf_port, SOCKET *the_socket){
	ZeroMemory(&si_type, sizeof (si_type));

	si_type.ai_family = AF_INET;
	si_type.ai_socktype = SOCK_STREAM;
	si_type.ai_protocol = IPPROTO_TCP;
	si_type.ai_flags = AI_PASSIVE;
	
   // Resolve the local address and port to be used by the server
	struct addrinfo *result = NULL;	

	int iResult = getaddrinfo(NULL, conf_port, &si_type, &result);
	if (iResult != 0) {
		printf("Socket: getaddrinfo failed: %d\n", iResult);
		WSACleanup();
		return 1;
	}

	*the_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);

	if (*the_socket == INVALID_SOCKET) {
		printf("Socket: socket failed [%ld]\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return 1;
	}

	iResult = bind(*the_socket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		printf("Socket: bind failed: %d\n", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(*the_socket);
		WSACleanup();
		return 1;
	}

	freeaddrinfo(result);

	if ( listen(*the_socket, SOMAXCONN ) == SOCKET_ERROR ) {
		printf( "Socket: listen failed [%ld]\n", WSAGetLastError() );
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

int sendMessage(int first, int second, unsigned char *data, int len) {
	int m_len = 1 + 1 + sizeof(int);
	unsigned char *msg = (unsigned char*) malloc(m_len + len);
	memcpy(msg, &first, 1);
	memcpy(msg + 1, &second, 1);
	memcpy(msg + 2, &len, sizeof(int));
	memcpy(msg + m_len, data, len);
	#ifdef WIN32
	int n = send(data_client_socket, (char*)msg, m_len + len, 0);
	#else
	int n = write(data_child, msg, m_len + len);
	#endif
	free((void*)msg);
	return n;
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

//send depth ARGB to client
void sendDepth(){
	uint32_t ts, x, y, i, j;
	freenect_sync_get_depth(&buf_depth_temp, &ts, 0, FREENECT_DEPTH_11BIT);
	uint16_t *depth = (uint16_t*) buf_depth_temp;
	uint16_t *tmp_depth = (uint16_t*) malloc(FREENECT_DEPTH_11BIT_SIZE);
	if(_depth_mirrored){	//MIRROR DEPTH DATA
		for(x = 0; x < FREENECT_FRAME_W; x++){
			for(y = 0; y < FREENECT_FRAME_H; y++){
				i = x + (y  * FREENECT_FRAME_W);
				j = (FREENECT_FRAME_W - x - 1) + (y  * FREENECT_FRAME_W);
				tmp_depth[i] = depth[j];
			}
		}
		depth = tmp_depth;
	}
	
	for (i=0; i<FREENECT_FRAME_PIX; i++) {
		buf_depth[4 * i + 0] = 0x00;
		buf_depth[4 * i + 1] = 0x00;
		buf_depth[4 * i + 2] = 0x00;
		buf_depth[4 * i + 3] = 0xFF;
		if(depth[i] < _max_depth && depth[i] > _min_depth){
			unsigned char l =  0xFF - ((depth[i] - _min_depth) & 0xFF);
			buf_depth[4 * i + 0] = l;
			buf_depth[4 * i + 1] = l;
			buf_depth[4 * i + 2] = l;
			buf_depth[4 * i + 3] = 0xFF;
		}
	}
	int n = sendMessage(0, 0, buf_depth, AS3_BITMAPDATA_LEN);
	if (n < 0)
	{
		printf("Error sending depth\n");
		client_connected = 0;
	}
	free(tmp_depth);
}

//send video ARGB to client
void sendVideo(){
	uint32_t ts,x, y, i, j;
	freenect_sync_get_video(&buf_rgb_temp, &ts, 0, FREENECT_VIDEO_RGB);
	uint8_t *rgb = (uint8_t*)buf_rgb_temp;

	//MIRROR RGB DATA AND ADD ALPHA
	for(x = 0; x < FREENECT_FRAME_W; x++){
		for(y = 0; y < FREENECT_FRAME_H; y++){
			i = x + (y  * FREENECT_FRAME_W);
			if(!_video_mirrored)
				j = i;
			else
				j = (FREENECT_FRAME_W - x - 1) + (y  * FREENECT_FRAME_W);
			buf_rgb[4 * i + 0] = rgb[3 * j + 2];
			buf_rgb[4 * i + 1] = rgb[3 * j + 1];
			buf_rgb[4 * i + 2] = rgb[3 * j + 0];
			buf_rgb[4 * i + 3] = 0x00;
		}
	}
	int n = sendMessage(0, 1, buf_rgb, AS3_BITMAPDATA_LEN);
	if ( n < 0)
	{
		printf("Error sending Video\n");
		client_connected = 0;
	}
}

//send accelerometer data to client
void sendAccelerometers() {
	int16_t ax,ay,az;
	double dx,dy,dz;
	unsigned char buffer_send[3*2+3*8];
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
	int n = sendMessage(1, 2, buffer_send, 3*2+3*8);
	if ( n < 0)
	{
		printf("Error sending Accelerometers\n");
		client_connected = 0;
	}
}

//Connection protocol handler
void *connection_handler(void *arg) {
	int n, value;
	//send_policy_file(data_child);
	while(client_connected) {
		char buff[8*10]; //Command buffer
		#ifdef WIN32 //Listen for data (Winsock)
			n = recv(data_client_socket, (char*)buff, sizeof(buff), 0);
		#else //Listen for data (UNIX)
			n = read(data_child, buff, sizeof(buff));
		#endif
		//If command length is multiple of 6
		if(n > 0 && n % 6 == 0){
			//Get the number of commands received
			int max = n / 6;
			int i;
			//For each command received
			for(i = 0; i < max; i++){
				memcpy(&value, &buff[2 + (i*6)], sizeof(int));
				value = ntohl(value);
				//The BIG switch (Communication protocol)
				switch(buff[0 + (i*6)]){
					case 0: //CAMERA
						switch(buff[1 + (i*6)]){
							case 0: //GET DEPTH
								sendDepth();
							break;
							case 1: //GET RGB
								sendVideo();
							break;
							case 2: //Mirror depth
								_depth_mirrored = value;
							break;
							case 3: //Mirror video
								_video_mirrored = value;
							break;
							case 4: //Min depth
								_min_depth = value;
							break;
							case 5: //Max depth
								_max_depth = value;
							break;
						}
					break;
					case 1: //MOTOR
						switch(buff[1 + (i*6)]){
							case 0: //MOVE
								freenect_sync_set_tilt_degs(value, 0);
							break;
							case 1: //LED COLOR
								freenect_sync_set_led((freenect_led_options) value, 0);
							break;
							case 2: //Accelerometer
								sendAccelerometers();
							break;
						}
					break;
				}
			}
		} else { //Command was not multiple of 6 (we received an invalid command)
			printf("got bad command (%d)\n", n);
			client_connected = 0;
		}
	}
	printf("Disconecting client...\n");
	//waiting for client led status
	freenect_sync_set_led((freenect_led_options) 4, 0);
	return NULL;
}

//Waiting for a client to connect
void *waiting_for_client(void *arg)
{
	int childlen;
	struct sockaddr_in childaddr;
	
	childlen = sizeof(childaddr);
	while (!die)
	{
		printf("### Wait client\n");
		#ifdef _WIN32 //Wait for connection (Winsock)
			data_client_socket = accept(data_socket, NULL, NULL);
			if (data_client_socket == INVALID_SOCKET) {
				if(!die) printf("Error on accept() for data, exit data thread. %d\n", WSAGetLastError());
				closesocket(data_socket);
				WSACleanup();
				break;
			}
		#else //Wait for connection (UNIX)
			data_child = accept(s_data, (struct sockaddr *)&childaddr, (unsigned int *)&childlen);
			if ( data_child < 0 )
			{
				if(!die) fprintf(stderr, "Error on accept() for data, exit data thread.\n");
				break;
			}
		#endif
		//Handle client connection and create connection thread to communicate with client
		printf("###### Got client\n");
		//got client led status
		freenect_sync_set_led((freenect_led_options) 1, 0);
		client_connected = 1;
		if (pthread_create(&connection_thread, NULL, &connection_handler, NULL)) {
			fprintf(stderr, "Error on pthread_create() for SERVER\n");
		}
		//Then we start listening again to allow re-connection to server
	}
	
	return NULL;
}

//Init network socket
int network_init()
{
	#ifndef _WIN32 //Init server UNIX
		signal(SIGPIPE, SIG_IGN);
		initServer(si_data, conf_port_data, &s_data); 
	#else //Init server Winsock (Windows)
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
		initServer(si_data, conf_port_data, &data_socket);
	#endif
	
	//Create thread to wait for connection
	if ( pthread_create(&wait_for_client_thread, NULL, waiting_for_client, NULL) )
	{
		fprintf(stderr, "Error on pthread_create() for data\n");
		return -1;
	}

	return 0;
}

//Close network socket
void network_close()
{
	die = 1;
#ifdef _WIN32
	if ( data_socket != INVALID_SOCKET )
		closesocket(data_socket);
#else
	if ( s_data != -1 )
		close(s_data), s_data = -1;
#endif
}

//Called when C-c or C-z is pressed
#ifdef _WIN32
void clean_exit(int){
	die = 1;
}
#else
void clean_exit(){
	die = 1;
}
#endif
//Main: we start here
int main(int argc, char **argv)
{
	printf("as3kinect server %s\n", _current_version);
	//waiting for client led status
	freenect_sync_set_led((freenect_led_options) 4, 0);
	//Listening to C-c
  	if (signal (SIGINT, clean_exit) == SIG_IGN)
    	signal (SIGINT, SIG_IGN);
	//Listening to C-z
	#ifndef _WIN32
  	if (signal (SIGTSTP, clean_exit) == SIG_IGN)
    	signal (SIGTSTP, SIG_IGN);
	#endif
	//Alloc memory for video and depth frames
	buf_depth_temp = malloc(FREENECT_DEPTH_11BIT);
	buf_rgb_temp = malloc(FREENECT_VIDEO_RGB);
	
	//Initialize network socket
	if ( network_init() < 0 )
		die = 1;
	
	//Sleep main process until exit
	while(!die)
		sleep(2);

	//making a clean exit
	free(buf_depth_temp);
	free(buf_rgb_temp);
	network_close();
	//device off led status
	freenect_sync_set_led((freenect_led_options) 0, 0);
	freenect_sync_stop();
	printf("\n[as3-server] cleanup done!\n");
	return 0;
}
