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
#include "libfreenect.h"
#include <pthread.h>
#include <math.h>

#include "freenect_network.h"
#include "as3_jpeg.h"
#include "libfreenect_sync.h"

int g_argc;
char **g_argv;

char *_current_version = "v0.9c";
pthread_t connection_thread;

#define AS3_BITMAPDATA_LEN 640 * 480 * 4

uint8_t buf_depth[AS3_BITMAPDATA_LEN];
uint8_t	buf_rgb[AS3_BITMAPDATA_LEN];
void *buf_depth_temp;
void *buf_rgb_temp;

int die = 0;
int psent = 0;

int _video_mirrored = 0,
	_depth_mirrored = 0,
	_min_depth = 600,
	_max_depth = 800,
	_video_compression = 80,
	_depth_compression = 20;
	
int client_connected = 0;

#ifdef _WIN32
	unsigned sleep(unsigned seconds)
	{
	  Sleep(seconds*1000);  // The Windows Sleep operates on milliseconds
	  return(0);
	}
#endif

void send_policy_file(int child){
	/*if(psent == 0){
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
	}*/
}

//send depth ARGB to client
void sendDepth(){
	int n;
	uint32_t ts, x, y, i, j;
	freenect_sync_get_depth(&buf_depth_temp, &ts, 0, FREENECT_DEPTH_11BIT);
	uint16_t *depth = (uint16_t*) buf_depth_temp;
	freenect_frame_mode depth_mode = freenect_find_depth_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_11BIT);
	//uint16_t *tmp_depth = (uint16_t*) malloc(FREENECT_DEPTH_11BIT_SIZE);
	uint16_t *tmp_depth = (uint16_t*) malloc(depth_mode.bytes);
	if(_depth_mirrored){	//MIRROR DEPTH DATA
		for(x = 0; x < depth_mode.width; x++){
			for(y = 0; y < depth_mode.height; y++){
				i = x + (y  * depth_mode.width);
				j = (depth_mode.width - x - 1) + (y  * depth_mode.width);
				tmp_depth[i] = depth[j];
			}
		}
		depth = tmp_depth;
	}
	
	for (i=0; i< depth_mode.width * depth_mode.height; i++) {
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
	if(_depth_compression != 0) {
		unsigned char *compressed_buff = (unsigned char *)malloc(AS3_BITMAPDATA_LEN);
		unsigned long len = 0;
		RGB_2_JPEG(buf_depth, &compressed_buff, &len, _depth_compression);
		n = freenect_network_sendMessage(0, 0, compressed_buff, (int)len);
		free(compressed_buff);
	} else {
		n = freenect_network_sendMessage(0, 0, buf_depth, AS3_BITMAPDATA_LEN);
	}
	if (n < 0)
	{
		printf("Error sending depth\n");
		client_connected = 0;
	}
	free(tmp_depth);
}

//send depth ARGB to client
void sendRawDepth(){
	uint32_t ts, x, y, i, j;
	freenect_sync_get_depth(&buf_depth_temp, &ts, 0, FREENECT_DEPTH_11BIT);
	uint16_t *depth = (uint16_t*) buf_depth_temp;
	freenect_frame_mode depth_mode = freenect_find_depth_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_11BIT);
	uint16_t *tmp_depth = (uint16_t*) malloc(depth_mode.bytes);
	unsigned char *char_depth = (unsigned char *) malloc(depth_mode.bytes);
	
	if(_depth_mirrored){	//MIRROR DEPTH DATA
		for(x = 0; x < depth_mode.width; x++){
			for(y = 0; y < depth_mode.height; y++){
				i = x + (y  * depth_mode.width);
				j = (depth_mode.width - x - 1) + (y  * depth_mode.width);
				tmp_depth[i] = depth[j];
			}
		}
		depth = tmp_depth;
	}
	
	for (i=0; i<640 * 480; i++) {
		memcpy(char_depth + (i*2), &depth[i], 2);
	}
	
	int n = freenect_network_sendMessage(0, 1, char_depth, depth_mode.bytes);
	if (n < 0)
	{
		printf("Error sending raw depth\n");
		client_connected = 0;
	}
	free(char_depth);
	free(tmp_depth);
}

//send video ARGB to client
void sendVideo(){
	int n;
	uint32_t ts,x, y, i, j;
	freenect_sync_get_video(&buf_rgb_temp, &ts, 0, FREENECT_VIDEO_RGB);
	uint8_t *rgb = (uint8_t*)buf_rgb_temp;
	freenect_frame_mode video_mode = freenect_find_video_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_RGB);

	//MIRROR RGB DATA AND ADD ALPHA
	for(x = 0; x < video_mode.width; x++){
		for(y = 0; y < video_mode.height; y++){
			i = x + (y  * video_mode.width);
			if(!_video_mirrored)
				j = i;
			else
				j = (video_mode.width - x - 1) + (y  * video_mode.width);
			if(_video_compression != 0) {
				buf_rgb[3 * i + 2] = rgb[3 * j + 2];
				buf_rgb[3 * i + 1] = rgb[3 * j + 1];
				buf_rgb[3 * i + 0] = rgb[3 * j + 0];
			} else {
				buf_rgb[4 * i + 0] = rgb[3 * j + 2];
				buf_rgb[4 * i + 1] = rgb[3 * j + 1];
				buf_rgb[4 * i + 2] = rgb[3 * j + 0];
				buf_rgb[4 * i + 3] = 0x00;	
			}
		}
	}
	if(_video_compression != 0) {
		unsigned char *compressed_buff = (unsigned char *)malloc(AS3_BITMAPDATA_LEN);
		unsigned long len = 0;
		RGB_2_JPEG(buf_rgb, &compressed_buff, &len, _video_compression);
		n = freenect_network_sendMessage(0, 2, compressed_buff, (int)len);
		free(compressed_buff);
	} else {
		n = freenect_network_sendMessage(0, 2, buf_rgb, AS3_BITMAPDATA_LEN);
	}
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
	int n = freenect_network_sendMessage(1, 2, buffer_send, 3*2+3*8);
	if ( n < 0)
	{
		printf("Error sending Accelerometers\n");
		client_connected = 0;
	}
}

//Connection protocol handler
void *connection_handler(void *arg) {
	int value;
	int len = 8*10;
	char *buff = (char*)malloc(len); //Command buffer
	//send_policy_file(data_child);
	while(client_connected) {
		freenect_network_read(buff, &len);
		//If command length is multiple of 6
		if(len > 0 && len % 6 == 0){
			//Get the number of commands received
			int max = len / 6;
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
							case 1: //GET RAW DEPTH
								sendRawDepth();
							break;
							case 2: //GET RGB
								sendVideo();
							break;
							case 3: //Mirror depth
								_depth_mirrored = value;
							break;
							case 4: //Mirror video
								_video_mirrored = value;
							break;
							case 5: //Min depth
								_min_depth = value;
							break;
							case 6: //Max depth
								_max_depth = value;
							break;
							case 7: //Depth compression
								_depth_compression = value;
							break;
							case 8: //Video compression
								_video_compression = value;
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
			if(!die) printf("got bad command (%d)\n", len	);
			client_connected = 0;
		}
	}
	if(!die) {
		printf("Disconecting client...\n");
		freenect_network_wait();
		//waiting for client led status
		freenect_sync_set_led((freenect_led_options) 4, 0);
	}
	return NULL;
}

void server_connected(){
	printf("###### Got client\n");
	//got client led status
	freenect_sync_set_led((freenect_led_options) 1, 0);
	client_connected = 1;
	if (pthread_create(&connection_thread, NULL, &connection_handler, NULL)) {
		fprintf(stderr, "Error on pthread_create() for SERVER\n");
	}
}

//Called when C-c or C-z is pressed
#ifdef _WIN32
void clean_exit(int){
	die = 1;
}
#else
void clean_exit(){
	printf("clean exit called\n");
	die = 1;
	freenect_network_close();
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
	if ( freenect_network_init(server_connected) < 0 )
		die = 1;
	
	//Sleep main process until exit
	while(!die)
		sleep(2);

	//making a clean exit
	free(buf_depth_temp);
	free(buf_rgb_temp);
	//device off led status
	freenect_sync_set_led((freenect_led_options) 0, 0);
	freenect_sync_stop();
	printf("\n[as3-server] cleanup done!\n");
	return 0;
}
