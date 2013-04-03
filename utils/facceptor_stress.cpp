/*
 * Copyright (c) 2011-2013, "Kira"
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met: 
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <error.h>

#define NUM_THREADS 500
#define LOOP_COUNT 5

static const char* policytext = "<?xml version=\"1.0\"?>\r\n<!DOCTYPE cross-domain-policy SYSTEM \"/xml/dtds/cross-domain-policy.dtd\">\r\n<cross-domain-policy>\r\n<site-control permitted-cross-domain-policies=\"master-only\"/>\r\n<allow-access-from domain=\"*\" to-ports=\"*\" />\r\n</cross-domain-policy>\r\n";
static const int policylen = 255;
static const char* requesttext = "<policy-file-request/>\0";
static const int requestlen = 23;
static const char* host_name = "127.0.0.1";

struct hostent* server;
struct sockaddr_in serv_addr;

static void* stress_acceptor_valid(void* loopcount)
{
	char buffer[1024];
	long count = 0;
	count = *(long*)loopcount;
	char reuse = 1;
	bzero(&buffer[0], sizeof(buffer));
	for(int t = 0; t < count;++t)
	{
		int sock = socket(AF_INET, SOCK_STREAM, 0);
		setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, 1);
		int ret = connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
		if ( ret < 0)
		{
			printf("Failed to connect to server. error: %s\n",  strerror(errno));
			close(sock);
			continue;
		}
		ret = send(sock, requesttext, requestlen, 0);
		if ( ret != requestlen)
		{
			printf("Failed to send whole request to server.\n");
			close(sock);
			continue;
		}
		ret = recv(sock, &buffer[0], policylen, 0);
		if ( ret == 0 )
		{
			printf("Server closed connection before we received any data.\n");
			close(sock);
			continue;
		}
		if ( ret != policylen )
		{
			printf("Failed to receive whole response from server. Len: %d:%d\n", ret, policylen);
			close(sock);
			continue;
		}
		if ( strncmp(&buffer[0], policytext, policylen) != 0)
		{
			printf("Received garbled response from server.\n");
			close(sock);
			continue;
		}
		else
		{
			//printf("Received valid response.\n");
		}
		close(sock);
	}
	printf("Finished thread.\n");
	pthread_exit(0);
}

int main(int argc, char* argv[])
{
	bzero(&serv_addr, sizeof(serv_addr));
	server = gethostbyname(host_name);
	serv_addr.sin_family = AF_INET;
	memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
	serv_addr.sin_port = htons(843);

	pthread_t threads[NUM_THREADS];
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	int loop_count = LOOP_COUNT;
	long i = 0;
	for(i = 0; i < NUM_THREADS; ++i)
	{
		pthread_create(&threads[i], &attr, stress_acceptor_valid, &loop_count);
	}

	for(i = 0;i < NUM_THREADS; ++i)
	{
		void* status = 0;
		pthread_join(threads[i], &status);
	}
	return 0;
}
