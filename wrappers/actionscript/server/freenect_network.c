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

#include "freenect_network.h"

int data_child;
int closing = 0;
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
	callback freenect_network_connected;
//Init network
int freenect_network_init(callback cb)
{
	#ifndef _WIN32 //Init server UNIX
		signal(SIGPIPE, SIG_IGN);
		freenect_network_initSocket(si_data, conf_port_data, &s_data); 
	#else //Init server Winsock (Windows)
		WORD wVersionRequested;
    	WSADATA wsaData;
    	int err;

    	wVersionRequested = MAKEWORD(2, 2);

    	err = WSAStartup(wVersionRequested, &wsaData);
    	if (err != 0) {
        	printf("WSAStartup failed with error: %d\n", err);
        	return 1;
    	}
		freenect_network_initSocket(si_data, conf_port_data, &data_socket);
	#endif
	freenect_network_connected = cb;
	freenect_network_wait();
		
	return 0;
}

void freenect_network_read(char *buff, int *len){
	int n;
	#ifdef _WIN32 //Listen for data (Winsock)
	n = recv(data_client_socket, buff, *len, 0);
	#else //Listen for data (UNIX)
	n = read(data_child, buff, *len);
	#endif
	*len = n;
}

// Send Message with two bytes as metadata and pkg len as the 3rd byte
int freenect_network_sendMessage(int first, int second, unsigned char *data, int len) {
	int n;
	int m_len = 1 + 1 + sizeof(int);
	unsigned char *msg = (unsigned char*) malloc(m_len + len);
	memcpy(msg, &first, 1);
	memcpy(msg + 1, &second, 1);
	memcpy(msg + 2, &len, sizeof(int));
	memcpy(msg + m_len, data, len);
	#ifdef WIN32
	n = send(data_client_socket, (char*)msg, m_len + len, 0);
	#else
	n = write(data_child, msg, m_len + len);
	#endif
	free((void*)msg);
	return n;
}

//Waiting for a client to connect
void freenect_network_wait()
{
	int childlen;
	struct sockaddr_in childaddr;
	
	childlen = sizeof(childaddr);

	printf("### Wait client\n");
	#ifdef _WIN32 //Wait for connection (Winsock)
		data_client_socket = accept(data_socket, NULL, NULL);
		if (data_client_socket == INVALID_SOCKET) {
			if(!closing) printf("Error on accept() for data, exit data thread. %d\n", WSAGetLastError());
			closesocket(data_socket);
			WSACleanup();
		}
	#else //Wait for connection (UNIX)
		data_child = accept(s_data, (struct sockaddr *)&childaddr, (unsigned int *)&childlen);
		if ( data_child < 0 )
		{
			if(!closing) fprintf(stderr, "Error on accept() for data, exit data thread.\n");
		}
	#endif
	
	//callback
	if(!closing) freenect_network_connected();
}

//Close network socket
void freenect_network_close()
{
	closing = 1;
#ifdef _WIN32
	if ( data_socket != INVALID_SOCKET )
		closesocket(data_socket);
#else
	if ( s_data != -1 )
		close(s_data), s_data = -1;
#endif
}

#ifdef _WIN32
int freenect_network_initSocket(struct addrinfo si_type, PCSTR conf_port, SOCKET *the_socket){
	struct addrinfo *result = NULL;	
	int iResult;
	ZeroMemory(&si_type, sizeof (si_type));

	si_type.ai_family = AF_INET;
	si_type.ai_socktype = SOCK_STREAM;
	si_type.ai_protocol = IPPROTO_TCP;
	si_type.ai_flags = AI_PASSIVE;

	iResult = getaddrinfo(NULL, conf_port, &si_type, &result);
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
int freenect_network_initSocket(struct sockaddr_in si_type, int conf_port, int *the_socket) {
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