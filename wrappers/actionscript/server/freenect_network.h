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

#ifndef FREENECT_SERVER_H
#define FREENECT_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

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
#endif
#include <string.h>
#include <signal.h>
#include <math.h>

	typedef void (*callback)(void);
	int freenect_network_init(callback cb);
	void freenect_network_close();
	void freenect_network_read(char *buff, int *len);
	int freenect_network_sendMessage(int first, int second, unsigned char *data, int len);
	void freenect_network_wait();
	#ifdef _WIN32
	int freenect_network_initSocket(struct addrinfo si_type, PCSTR conf_port, SOCKET *the_socket);
	#else 
	int freenect_network_initSocket(struct sockaddr_in si_type, int conf_port, int *the_socket);
	#endif

#ifdef __cplusplus
}
#endif

#endif