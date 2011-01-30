/*
 * Copyright (c) 1998-2009 by the citadel.org team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  Inspired by NodeJS.org; thanks for the MX-Parser ;-)
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <termios.h>
#include <fcntl.h>
#include <signal.h>
#include <pwd.h>
#include <errno.h>
#include <sys/types.h>
#include <syslog.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#include <sys/wait.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "support.h"

#include "ctdl_module.h"
#include "event_client.h"


extern struct ev_loop *event_base;

void SockStateCb(void *data, int sock, int read, int write);


static void HostByAddrCb(void *data,
                         int status,
                         int timeouts,
                         struct hostent *hostent) 
{
	AsyncIO *IO = data;
	IO->DNSStatus = status;
	if  (status != ARES_SUCCESS) {
//		ResolveError(*cb, status);
		return;
	}
	IO->Data = hostent;
/// TODO: howto free this??
}

static void ParseAnswerA(AsyncIO *IO, unsigned char* abuf, int alen) 
{
	struct hostent* host;

	if (IO->VParsedDNSReply != NULL)
		IO->DNSReplyFree(IO->VParsedDNSReply);
	IO->VParsedDNSReply = NULL;

	IO->DNSStatus = ares_parse_a_reply(abuf, alen, &host, NULL, NULL);
	if (IO->DNSStatus != ARES_SUCCESS) {
//    ResolveError(arg->js_cb, status);
		return;
	}
	IO->VParsedDNSReply = host;
	IO->DNSReplyFree = (FreeDNSReply) ares_free_hostent;
}


static void ParseAnswerAAAA(AsyncIO *IO, unsigned char* abuf, int alen) 
{
	struct hostent* host;

	if (IO->VParsedDNSReply != NULL)
		IO->DNSReplyFree(IO->VParsedDNSReply);
	IO->VParsedDNSReply = NULL;

	IO->DNSStatus = ares_parse_aaaa_reply(abuf, alen, &host, NULL, NULL);
	if (IO->DNSStatus != ARES_SUCCESS) {
//    ResolveError(arg->js_cb, status);
		return;
	}
	IO->VParsedDNSReply = host;
	IO->DNSReplyFree = (FreeDNSReply) ares_free_hostent;
}


static void ParseAnswerCNAME(AsyncIO *IO, unsigned char* abuf, int alen) 
{
	struct hostent* host;

	if (IO->VParsedDNSReply != NULL)
		IO->DNSReplyFree(IO->VParsedDNSReply);
	IO->VParsedDNSReply = NULL;

	IO->DNSStatus = ares_parse_a_reply(abuf, alen, &host, NULL, NULL);
	if (IO->DNSStatus != ARES_SUCCESS) {
//    ResolveError(arg->js_cb, status);
		return;
	}

	// a CNAME lookup always returns a single record but
	IO->VParsedDNSReply = host;
	IO->DNSReplyFree = (FreeDNSReply) ares_free_hostent;
}


static void ParseAnswerMX(AsyncIO *IO, unsigned char* abuf, int alen) 
{
	struct ares_mx_reply *mx_out;

	if (IO->VParsedDNSReply != NULL)
		IO->DNSReplyFree(IO->VParsedDNSReply);
	IO->VParsedDNSReply = NULL;

	IO->DNSStatus = ares_parse_mx_reply(abuf, alen, &mx_out);
	if (IO->DNSStatus != ARES_SUCCESS) {
//    ResolveError(arg->js_cb, status);
		return;
	}

	IO->VParsedDNSReply = mx_out;
	IO->DNSReplyFree = (FreeDNSReply) ares_free_data;
}


static void ParseAnswerNS(AsyncIO *IO, unsigned char* abuf, int alen) 
{
	struct hostent* host;

	if (IO->VParsedDNSReply != NULL)
		IO->DNSReplyFree(IO->VParsedDNSReply);
	IO->VParsedDNSReply = NULL;

	IO->DNSStatus = ares_parse_ns_reply(abuf, alen, &host);
	if (IO->DNSStatus != ARES_SUCCESS) {
//    ResolveError(arg->js_cb, status);
		return;
	}
	IO->VParsedDNSReply = host;
	IO->DNSReplyFree = (FreeDNSReply) ares_free_hostent;
}


static void ParseAnswerSRV(AsyncIO *IO, unsigned char* abuf, int alen) 
{
	struct ares_srv_reply *srv_out;

	if (IO->VParsedDNSReply != NULL)
		IO->DNSReplyFree(IO->VParsedDNSReply);
	IO->VParsedDNSReply = NULL;

	IO->DNSStatus = ares_parse_srv_reply(abuf, alen, &srv_out);
	if (IO->DNSStatus != ARES_SUCCESS) {
//    ResolveError(arg->js_cb, status);
		return;
	}

	IO->VParsedDNSReply = srv_out;
	IO->DNSReplyFree = (FreeDNSReply) ares_free_data;
}


static void ParseAnswerTXT(AsyncIO *IO, unsigned char* abuf, int alen) 
{
	struct ares_txt_reply *txt_out;

	if (IO->VParsedDNSReply != NULL)
		IO->DNSReplyFree(IO->VParsedDNSReply);
	IO->VParsedDNSReply = NULL;

	IO->DNSStatus = ares_parse_txt_reply(abuf, alen, &txt_out);
	if (IO->DNSStatus != ARES_SUCCESS) {
//    ResolveError(arg->js_cb, status);
		return;
	}
	IO->VParsedDNSReply = txt_out;
	IO->DNSReplyFree = (FreeDNSReply) ares_free_data;
}

void QueryCb(void *arg,
	     int status,
	     int timeouts,
	     unsigned char* abuf,
	     int alen) 
{
	AsyncIO *IO = arg;

	IO->DNSStatus = status;
	if (status == ARES_SUCCESS)
		IO->DNS_CB(arg, abuf, alen);
	else
		IO->DNSStatus = status;
///	ev_io_stop(event_base, &IO->dns_io_event);
	
	ev_timer_init(&IO->unwind_stack_timeout,
		      IO_postdns_callback, 0.0, 0);
	IO->unwind_stack_timeout.data = IO;
	ev_timer_start(event_base, &IO->unwind_stack_timeout);
}

void QueryCbDone(AsyncIO *IO)
{
	ev_timer_stop(event_base, &IO->unwind_stack_timeout);
}


void InitC_ares_dns(AsyncIO *IO)
{
	int optmask = 0;
	if (IO->DNSChannel == NULL) {
		optmask |= ARES_OPT_SOCK_STATE_CB;
		IO->DNSOptions.sock_state_cb = SockStateCb;
		IO->DNSOptions.sock_state_cb_data = IO;
		ares_init_options(&IO->DNSChannel, &IO->DNSOptions, optmask);
	}
}
int QueueQuery(ns_type Type, char *name, AsyncIO *IO, IO_CallBack PostDNS)
{
	int length, family;
	char address_b[sizeof(struct in6_addr)];

	InitC_ares_dns(IO);
	IO->PostDNS = PostDNS;
	switch(Type) {
	case ns_t_a:
		IO->DNS_CB = ParseAnswerA;
		break;

	case ns_t_aaaa:
		IO->DNS_CB = ParseAnswerAAAA;
		break;

	case ns_t_mx:
		IO->DNS_CB = ParseAnswerMX;
		break;

	case ns_t_ns:
		IO->DNS_CB = ParseAnswerNS;
		break;

	case ns_t_txt:
		IO->DNS_CB = ParseAnswerTXT;
		break;

	case ns_t_srv:
		IO->DNS_CB = ParseAnswerSRV;
		break;

	case ns_t_cname:
		IO->DNS_CB = ParseAnswerCNAME;
		break;

	case ns_t_ptr:


		if (inet_pton(AF_INET, name, &address_b) == 1) {
			length = sizeof(struct in_addr);
			family = AF_INET;
		} else if (inet_pton(AF_INET6, name, &address_b) == 1) {
			length = sizeof(struct in6_addr);
			family = AF_INET6;
		} else {
			return -1;
		}

		ares_gethostbyaddr(IO->DNSChannel, address_b, length, family, HostByAddrCb, IO);

		return 1;

	default:
		return 0;
	}
	ares_query(IO->DNSChannel, name, ns_c_in, Type, QueryCb, IO);
	return 1;
}

static void DNS_send_callback(struct ev_loop *loop, ev_io *watcher, int revents)
{
	AsyncIO *IO = watcher->data;
	
	ares_process_fd(IO->DNSChannel, ARES_SOCKET_BAD, IO->dns_send_event.fd);
}
static void DNS_recv_callback(struct ev_loop *loop, ev_io *watcher, int revents)
{
	AsyncIO *IO = watcher->data;
	
	ares_process_fd(IO->DNSChannel, IO->dns_recv_event.fd, ARES_SOCKET_BAD);
}

void SockStateCb(void *data, int sock, int read, int write) 
{
/*
	struct timeval tvbuf, maxtv, *ret;
	
	int64_t time = 10;
*/
	AsyncIO *IO = data;
/* already inside of the event queue. */	

	if (read) {
		if ((IO->dns_recv_event.fd != sock) &&
		    (IO->dns_recv_event.fd != 0) && 
		    ((IO->active_dns_event & EV_READ) != 0)) {
			ev_io_stop(event_base, &IO->dns_recv_event);
		}
		IO->dns_recv_event.fd = sock;
		ev_io_init(&IO->dns_recv_event, DNS_recv_callback, IO->dns_recv_event.fd, EV_READ);
		IO->dns_recv_event.data = IO;
		ev_io_start(event_base, &IO->dns_recv_event);
		IO->active_dns_event = IO->active_dns_event | EV_READ;
	} 
	if (write) {
		if ((IO->dns_send_event.fd != sock) &&
		    (IO->dns_send_event.fd != 0) && 
		    ((IO->active_dns_event & EV_WRITE) != 0)) {
			ev_io_stop(event_base, &IO->dns_send_event);
		}
		IO->dns_send_event.fd = sock;
		ev_io_init(&IO->dns_send_event, DNS_send_callback, IO->dns_send_event.fd, EV_WRITE);
		IO->dns_send_event.data = IO;
		ev_io_start(event_base, &IO->dns_send_event);
		IO->active_dns_event = IO->active_dns_event | EV_WRITE;
	}
/*

		ev_io_start(event_base, &IO->dns_io_event);
	
		maxtv.tv_sec = time/1000;
		maxtv.tv_usec = (time % 1000) * 1000;
		
		ret = ares_timeout(IO->DNSChannel, &maxtv, &tvbuf);
	}
*/
	if ((read == 0) && (write == 0)) {
		if ((IO->active_dns_event & EV_READ) != 0)
			ev_io_stop(event_base, &IO->dns_recv_event);
		if ((IO->active_dns_event & EV_WRITE) != 0)
			ev_io_stop(event_base, &IO->dns_send_event);
		IO->active_dns_event = 0;
	}
}

CTDL_MODULE_INIT(c_ares_client)
{
	if (!threading)
	{
		int r = ares_library_init(ARES_LIB_INIT_ALL);
		if (0 != r) {
			// TODO
			// ThrowException(Exception::Error(String::New(ares_strerror(r))));
////			assert(r == 0);
		}
	}
	return "c-ares";
}
