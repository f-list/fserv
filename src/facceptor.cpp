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
#include <fcntl.h>
#include <errno.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ev.h>
#include <signal.h>

int listensock=-1;
struct sockaddr_in serv_addr, client_addr;
struct ev_loop* main_loop = 0;

static const char* policytext = "<?xml version=\"1.0\"?>\r\n<!DOCTYPE cross-domain-policy SYSTEM \"/xml/dtds/cross-domain-policy.dtd\">\r\n<cross-domain-policy>\r\n<site-control permitted-cross-domain-policies=\"master-only\"/>\r\n<allow-access-from domain=\"*\" to-ports=\"*\" />\r\n</cross-domain-policy>\r\n";
static const size_t policylen = 255;
static const char* requesttext = "<policy-file-request/>";
static const size_t requestlen = 22;

typedef struct
{
	int fd;
	char buffer[1024];
	size_t pos;
	bool has_sent;
	ev_io* io;
	ev_timer* timer;
} acceptor_conn;

static void setup_signals()
{
	signal(SIGHUP, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
}

static void set_nonblock(int socket)
{
	int flags = 0;
	flags = fcntl(socket,F_GETFL,0);
	if ( flags != -1)
		fcntl(socket, F_SETFL, flags | O_NONBLOCK);
}

static void close_conn(acceptor_conn* con)
{
	ev_io_stop(main_loop, con->io);
	delete con->io;
	ev_timer_stop(main_loop, con->timer);
	delete con->timer;
	close(con->fd);
	delete con;
}

static void timeout_cb(struct ev_loop* loop, ev_timer* w, int revents)
{
	//printf("Timed out waiting for a connection to ask for its data.\n");
	acceptor_conn* con = (acceptor_conn*)w->data;
	close_conn(con);
}

static void connection_cb(struct ev_loop* loop, ev_io* w, int revents)
{
	acceptor_conn* con = (acceptor_conn*)w->data;
	if ( revents & EV_READ )
	{
		ssize_t recvd = recv(w->fd, &con->buffer[con->pos], 1023 - con->pos, 0);
		if ( recvd < 0 && (errno == EAGAIN || errno == EWOULDBLOCK) )
			return;
		if ( recvd <= 0 )
		{
			close_conn(con);
			return;
		}
		con->pos += recvd;
		if ( con->pos > 1023 )
		{
			close_conn(con);
			return;
		}
		if ( con->pos >= requestlen )
		{
			if( strncmp(&con->buffer[0], requesttext, requestlen) == 0)
			{
				ev_io_set(w, w->fd, EV_READ | EV_WRITE );
				ev_io_stop(loop, w);
				ev_io_start(loop, w);
				ssize_t sent = send(w->fd, policytext, policylen, 0);
				if ( sent >= 0 )
					con->has_sent = true;
			}
		}
	}

	if ( revents & EV_WRITE )
	{
		if ( con->has_sent )
		{
			close_conn(con);
			return;
		}
		else
		{
			ssize_t sent = send(w->fd, policytext, policylen, 0);
			if ( sent >= 0 )
				con->has_sent = true;
		}
	}
}


static void listen_cb (struct ev_loop* loop, ev_io* w, int revents)
{
	if( !( revents & EV_READ ) )
		return;
	int socklen = sizeof(client_addr);
	int newfd = accept(w->fd, (sockaddr*)&client_addr, (socklen_t*)&socklen);
	if( newfd > 0)
	{
		set_nonblock(newfd);
		ev_io* new_io = new ev_io;
		ev_timer* new_timer = new ev_timer;
		ev_io_init(new_io, connection_cb, newfd, EV_READ);
		ev_timer_init(new_timer, timeout_cb, 12., 0.);
		acceptor_conn* new_con = new acceptor_conn;
		new_con->io = new_io;
		new_con->timer = new_timer;
		new_con->fd = newfd;
		new_con->pos = 0;
		bzero(&new_con->buffer[0], 1024);
		new_con->has_sent = false;
		new_io->data = new_con;
		new_timer->data = new_con;
		ev_io_start(loop, new_io);
		ev_timer_start(loop, new_timer);
	}
}

int main(int argc, char* argv[])
{
	//printf("%d:%d\n", (int)strlen(policytext), (int)strlen(requesttext));
	setup_signals();
	main_loop = ev_default_loop(0);
	if(!main_loop)
	{
		printf("Could not create an event loop.\n");
		exit(-1);
	}
	//ev_set_io_collect_interval(main_loop, 0.005);
	ev_set_timeout_collect_interval(main_loop, 0.1);

	listensock = socket(AF_INET, SOCK_STREAM, 0);
	char re_use = 1;
	setsockopt(listensock, SOL_SOCKET, SO_REUSEADDR, &re_use, 1);
	set_nonblock(listensock);
	bzero((char*)&serv_addr, sizeof(serv_addr));
	bzero((char*)&client_addr, sizeof(client_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(843);
	if(bind(listensock, (const sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
	{
		printf("Could not bind to server socket.\n");
		exit(-1);
	}

	if(listen(listensock, 10) < 0)
	{
		printf("Could not listen on server socket.\n");
		exit(-1);
	}

	ev_io ev_listen;
	ev_io_init(&ev_listen, listen_cb, listensock, EV_READ);
	ev_io_start(main_loop, &ev_listen);
	ev_loop(main_loop, 0);
    return 0;
}
