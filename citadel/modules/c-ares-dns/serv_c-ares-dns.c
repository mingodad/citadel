/*
 * Copyright (c) 1998-2012 by the citadel.org team
 *
 *  This program is open source software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 *  
 *  
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  
 *  
 *  
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

int DebugCAres = 0;

extern struct ev_loop *event_base;

void SockStateCb(void *data, int sock, int read, int write);


static void HostByAddrCb(void *data,
			 int status,
			 int timeouts,
			 struct hostent *hostent)
{
	AsyncIO *IO = data;

	EV_DNS_syslog(LOG_DEBUG, "C-ARES: %s\n", __FUNCTION__);

	EV_DNS_LOGT_STOP(DNS.timeout);
	ev_timer_stop (event_base, &IO->DNS.timeout);

	IO->DNS.Query->DNSStatus = status;
	if  (status != ARES_SUCCESS) {
		StrBufPlain(IO->ErrMsg, ares_strerror(status), -1);
		return;
	}
	IO->DNS.Query->Data = hostent;
}

static void ParseAnswerA(AsyncIO *IO, unsigned char* abuf, int alen)
{
	struct hostent* host = NULL;

	EV_DNS_syslog(LOG_DEBUG, "C-ARES: %s\n", __FUNCTION__);

	if (IO->DNS.Query->VParsedDNSReply != NULL)
		IO->DNS.Query->DNSReplyFree(IO->DNS.Query->VParsedDNSReply);
	IO->DNS.Query->VParsedDNSReply = NULL;

	IO->DNS.Query->DNSStatus = ares_parse_a_reply(abuf,
						      alen,
						      &host,
						      NULL,
						      NULL);
	if (IO->DNS.Query->DNSStatus != ARES_SUCCESS) {
		if (host != NULL)
			ares_free_hostent(host);
		StrBufPlain(IO->ErrMsg,
			    ares_strerror(IO->DNS.Query->DNSStatus), -1);
		return;
	}
	IO->DNS.Query->VParsedDNSReply = host;
	IO->DNS.Query->DNSReplyFree = (FreeDNSReply) ares_free_hostent;
}


static void ParseAnswerAAAA(AsyncIO *IO, unsigned char* abuf, int alen)
{
	struct hostent* host = NULL;

	EV_DNS_syslog(LOG_DEBUG, "C-ARES: %s\n", __FUNCTION__);

	if (IO->DNS.Query->VParsedDNSReply != NULL)
		IO->DNS.Query->DNSReplyFree(IO->DNS.Query->VParsedDNSReply);
	IO->DNS.Query->VParsedDNSReply = NULL;

	IO->DNS.Query->DNSStatus = ares_parse_aaaa_reply(abuf,
							 alen,
							 &host,
							 NULL,
							 NULL);
	if (IO->DNS.Query->DNSStatus != ARES_SUCCESS) {
		if (host != NULL)
			ares_free_hostent(host);
		StrBufPlain(IO->ErrMsg,
			    ares_strerror(IO->DNS.Query->DNSStatus), -1);
		return;
	}
	IO->DNS.Query->VParsedDNSReply = host;
	IO->DNS.Query->DNSReplyFree = (FreeDNSReply) ares_free_hostent;
}


static void ParseAnswerCNAME(AsyncIO *IO, unsigned char* abuf, int alen)
{
	struct hostent* host = NULL;

	EV_DNS_syslog(LOG_DEBUG, "C-ARES: %s\n", __FUNCTION__);

	if (IO->DNS.Query->VParsedDNSReply != NULL)
		IO->DNS.Query->DNSReplyFree(IO->DNS.Query->VParsedDNSReply);
	IO->DNS.Query->VParsedDNSReply = NULL;

	IO->DNS.Query->DNSStatus = ares_parse_a_reply(abuf,
						      alen,
						      &host,
						      NULL,
						      NULL);
	if (IO->DNS.Query->DNSStatus != ARES_SUCCESS) {
		if (host != NULL)
			ares_free_hostent(host);
		StrBufPlain(IO->ErrMsg,
			    ares_strerror(IO->DNS.Query->DNSStatus), -1);
		return;
	}

	// a CNAME lookup always returns a single record but
	IO->DNS.Query->VParsedDNSReply = host;
	IO->DNS.Query->DNSReplyFree = (FreeDNSReply) ares_free_hostent;
}


static void ParseAnswerMX(AsyncIO *IO, unsigned char* abuf, int alen)
{
	struct ares_mx_reply *mx_out = NULL;

	EV_DNS_syslog(LOG_DEBUG, "C-ARES: %s\n", __FUNCTION__);

	if (IO->DNS.Query->VParsedDNSReply != NULL)
		IO->DNS.Query->DNSReplyFree(IO->DNS.Query->VParsedDNSReply);
	IO->DNS.Query->VParsedDNSReply = NULL;

	IO->DNS.Query->DNSStatus = ares_parse_mx_reply(abuf, alen, &mx_out);
	if (IO->DNS.Query->DNSStatus != ARES_SUCCESS) {
		if (mx_out != NULL)
			ares_free_data(mx_out);
		StrBufPlain(IO->ErrMsg,
			    ares_strerror(IO->DNS.Query->DNSStatus), -1);
		return;
	}

	IO->DNS.Query->VParsedDNSReply = mx_out;
	IO->DNS.Query->DNSReplyFree = (FreeDNSReply) ares_free_data;
}


static void ParseAnswerNS(AsyncIO *IO, unsigned char* abuf, int alen)
{
	struct hostent* host = NULL;

	EV_DNS_syslog(LOG_DEBUG, "C-ARES: %s\n", __FUNCTION__);

	if (IO->DNS.Query->VParsedDNSReply != NULL)
		IO->DNS.Query->DNSReplyFree(IO->DNS.Query->VParsedDNSReply);
	IO->DNS.Query->VParsedDNSReply = NULL;

	IO->DNS.Query->DNSStatus = ares_parse_ns_reply(abuf, alen, &host);
	if (IO->DNS.Query->DNSStatus != ARES_SUCCESS) {
		if (host != NULL)
			ares_free_hostent(host);
		StrBufPlain(IO->ErrMsg,
			    ares_strerror(IO->DNS.Query->DNSStatus), -1);
		return;
	}
	IO->DNS.Query->VParsedDNSReply = host;
	IO->DNS.Query->DNSReplyFree = (FreeDNSReply) ares_free_hostent;
}


static void ParseAnswerSRV(AsyncIO *IO, unsigned char* abuf, int alen)
{
	struct ares_srv_reply *srv_out = NULL;

	EV_DNS_syslog(LOG_DEBUG, "C-ARES: %s\n", __FUNCTION__);

	if (IO->DNS.Query->VParsedDNSReply != NULL)
		IO->DNS.Query->DNSReplyFree(IO->DNS.Query->VParsedDNSReply);
	IO->DNS.Query->VParsedDNSReply = NULL;

	IO->DNS.Query->DNSStatus = ares_parse_srv_reply(abuf, alen, &srv_out);
	if (IO->DNS.Query->DNSStatus != ARES_SUCCESS) {
		if (srv_out != NULL)
			ares_free_data(srv_out);
		StrBufPlain(IO->ErrMsg,
			    ares_strerror(IO->DNS.Query->DNSStatus), -1);
		return;
	}

	IO->DNS.Query->VParsedDNSReply = srv_out;
	IO->DNS.Query->DNSReplyFree = (FreeDNSReply) ares_free_data;
}


static void ParseAnswerTXT(AsyncIO *IO, unsigned char* abuf, int alen)
{
	struct ares_txt_reply *txt_out;

	EV_DNS_syslog(LOG_DEBUG, "C-ARES: %s\n", __FUNCTION__);

	if (IO->DNS.Query->VParsedDNSReply != NULL)
		IO->DNS.Query->DNSReplyFree(IO->DNS.Query->VParsedDNSReply);
	IO->DNS.Query->VParsedDNSReply = NULL;

	IO->DNS.Query->DNSStatus = ares_parse_txt_reply(abuf, alen, &txt_out);
	if (IO->DNS.Query->DNSStatus != ARES_SUCCESS) {
		if (txt_out != NULL)
			ares_free_data(txt_out);
		StrBufPlain(IO->ErrMsg,
			    ares_strerror(IO->DNS.Query->DNSStatus), -1);
		return;
	}
	IO->DNS.Query->VParsedDNSReply = txt_out;
	IO->DNS.Query->DNSReplyFree = (FreeDNSReply) ares_free_data;
}

void QueryCb(void *arg,
	     int status,
	     int timeouts,
	     unsigned char* abuf,
	     int alen)
{
	AsyncIO *IO = arg;

	EV_DNS_syslog(LOG_DEBUG, "C-ARES: %s\n", __FUNCTION__);

	EV_DNS_LOGT_STOP(DNS.timeout);
	ev_timer_stop (event_base, &IO->DNS.timeout);

	IO->DNS.Query->DNSStatus = status;
	if (status == ARES_SUCCESS)
		IO->DNS.Query->DNS_CB(arg, abuf, alen);
	else {
		EV_DNS_syslog(LOG_DEBUG, "C-ARES: Failed by: %s error %s\n",
			      __FUNCTION__,
			      ares_strerror(status));
		StrBufPrintf(IO->ErrMsg,
			     "%s-lookup %s - %s",
			     IO->DNS.Query->QueryTYPE,
			     (IO->DNS.Query->QStr != NULL)? IO->DNS.Query->QStr : "",
			     ares_strerror(status));
		IO->DNS.Query->DNSStatus = status;
	}

	ev_idle_init(&IO->unwind_stack,
		     IO_postdns_callback);
	IO->unwind_stack.data = IO;
	EV_DNS_LOGT_INIT(unwind_stack);
	EV_DNS_LOGT_START(unwind_stack);
	ev_idle_start(event_base, &IO->unwind_stack);
}

void QueryCbDone(AsyncIO *IO)
{
	EV_DNS_syslog(LOG_DEBUG, "C-ARES: %s\n", __FUNCTION__);

	EV_DNS_LOGT_STOP(DNS.timeout);
	ev_timer_stop (event_base, &IO->DNS.timeout);

	EV_DNS_LOGT_STOP(unwind_stack);
	ev_idle_stop(event_base, &IO->unwind_stack);
}

void DestructCAres(AsyncIO *IO)
{
	EVNC_syslog(LOG_DEBUG, "C-ARES: %s\n", __FUNCTION__);

	EVNC_syslog(LOG_DEBUG, "C-ARES: - stopping %s %d %p \n", "DNS.recv_event", IO->DNS.recv_event.fd, &IO->DNS.recv_event);
	ev_io_stop(event_base, &IO->DNS.recv_event);

	EVNC_syslog(LOG_DEBUG, "C-ARES: - stopping %s %d %p\n", "DNS.send_event", IO->DNS.send_event.fd, &IO->DNS.send_event);
	ev_io_stop(event_base, &IO->DNS.send_event);

	EVNC_syslog(LOG_DEBUG, "C-ARES: - stopping %s %p\n", "DNS.timeout", &IO->DNS.send_event);
	ev_timer_stop (event_base, &IO->DNS.timeout);

	EVNC_syslog(LOG_DEBUG, "C-ARES: - stopping %s %p\n", "DNS.unwind_stack", &IO->unwind_stack);
	ev_idle_stop(event_base, &IO->unwind_stack);
	ares_destroy_options(&IO->DNS.Options);
}


void InitC_ares_dns(AsyncIO *IO)
{
	int optmask = 0;

	EV_DNS_syslog(LOG_DEBUG, "C-ARES: %s %p\n", __FUNCTION__, IO->DNS.Channel);

	if (IO->DNS.Channel == NULL) {
		optmask |= ARES_OPT_SOCK_STATE_CB;
		IO->DNS.Options.sock_state_cb = SockStateCb;
		IO->DNS.Options.sock_state_cb_data = IO;
		ares_init_options(&IO->DNS.Channel, &IO->DNS.Options, optmask);
	}
	IO->DNS.Query->DNSStatus = 0;
}

static void
DNStimeouttrigger_callback(struct ev_loop *loop, ev_timer *watcher, int revents)
{
	AsyncIO *IO = watcher->data;
	struct timeval tv, MaxTV;
	struct timeval *NextTV;

	memset(&MaxTV, 0, sizeof(MaxTV));
	memset(&tv, 0, sizeof(tv));
	MaxTV.tv_sec = 30;
	NextTV = ares_timeout(IO->DNS.Channel, &MaxTV, &tv);

	if ((NextTV->tv_sec != MaxTV.tv_sec) ||
	    (NextTV->tv_usec != MaxTV.tv_usec))
	{
		fd_set readers, writers;
		EV_DNS_syslog(LOG_DEBUG, "C-ARES: %s Timeout!\n", __FUNCTION__);

		FD_ZERO(&readers);
		FD_ZERO(&writers);
		ares_fds(IO->DNS.Channel, &readers, &writers);
		ares_process(IO->DNS.Channel, &readers, &writers);
	}
}

void QueueGetHostByNameDone(void *Ctx,
			    int status,
			    int timeouts,
			    struct hostent *hostent)
{
	AsyncIO *IO = (AsyncIO *) Ctx;

	EV_DNS_syslog(LOG_DEBUG, "C-ARES: %s\n", __FUNCTION__);


	IO->DNS.Query->DNSStatus = status;
	IO->DNS.Query->VParsedDNSReply = hostent;
	IO->DNS.Query->DNSReplyFree = (FreeDNSReply) ares_free_hostent;

	EV_DNS_LOGT_STOP(DNS.timeout);
	ev_timer_stop (event_base, &IO->DNS.timeout);

	ev_idle_init(&IO->unwind_stack,
		     IO_postdns_callback);
	IO->unwind_stack.data = IO;
	EV_DNS_LOGT_INIT(unwind_stack);
	EV_DNS_LOGT_START(unwind_stack);
	ev_idle_start(event_base, &IO->unwind_stack);

}

void QueueGetHostByName(AsyncIO *IO,
			const char *Hostname,
			DNSQueryParts *QueryParts,
			IO_CallBack PostDNS)
{

	EV_DNS_syslog(LOG_DEBUG, "C-ARES: %s\n", __FUNCTION__);
	IO->DNS.SourcePort = 0;

	IO->DNS.Query = QueryParts;
	IO->DNS.Query->PostDNS = PostDNS;

	InitC_ares_dns(IO);

	ev_timer_init(&IO->DNS.timeout, DNStimeouttrigger_callback, 10, 1);
	EV_DNS_LOGT_INIT(DNS.timeout);
	IO->DNS.timeout.data = IO;
	ares_gethostbyname(IO->DNS.Channel,
			   Hostname,
			   AF_INET6, /* it falls back to ipv4 in doubt... */
			   QueueGetHostByNameDone,
			   IO);
	EV_DNS_LOGT_START(DNS.timeout);
	ev_timer_start(event_base, &IO->DNS.timeout);

}

const char* QT[] = {
	"A",
	"AAAA",
	"MX",
	"NS",
	"TXT",
	"SRV",
	"CNAME",
	"PTR"
};

int QueueQuery(ns_type Type,
	       const char *name,
	       AsyncIO *IO,
	       DNSQueryParts *QueryParts,
	       IO_CallBack PostDNS)
{
	int length, family;
	char address_b[sizeof(struct in6_addr)];

	IO->DNS.SourcePort = 0;

	IO->DNS.Query = QueryParts;
	IO->DNS.Query->PostDNS = PostDNS;
	IO->DNS.Start = IO->Now;
	IO->DNS.Query->QStr = name;

	InitC_ares_dns(IO);

	ev_timer_init(&IO->DNS.timeout, DNStimeouttrigger_callback, 10, 1);
	IO->DNS.timeout.data = IO;
	EV_DNS_LOGT_INIT(DNS.timeout);

	switch(Type) {
	case ns_t_a:
		IO->DNS.Query->DNS_CB = ParseAnswerA;
		IO->DNS.Query->QueryTYPE = QT[0];
		break;

	case ns_t_aaaa:
		IO->DNS.Query->DNS_CB = ParseAnswerAAAA;
		IO->DNS.Query->QueryTYPE = QT[1];
		break;

	case ns_t_mx:
		IO->DNS.Query->DNS_CB = ParseAnswerMX;
		IO->DNS.Query->QueryTYPE = QT[2];
		break;

	case ns_t_ns:
		IO->DNS.Query->DNS_CB = ParseAnswerNS;
		IO->DNS.Query->QueryTYPE = QT[3];
		break;

	case ns_t_txt:
		IO->DNS.Query->DNS_CB = ParseAnswerTXT;
		IO->DNS.Query->QueryTYPE = QT[4];
		break;

	case ns_t_srv:
		IO->DNS.Query->DNS_CB = ParseAnswerSRV;
		IO->DNS.Query->QueryTYPE = QT[5];
		break;

	case ns_t_cname:
		IO->DNS.Query->DNS_CB = ParseAnswerCNAME;
		IO->DNS.Query->QueryTYPE = QT[6];
		break;

	case ns_t_ptr:
		IO->DNS.Query->QueryTYPE = QT[7];
		if (inet_pton(AF_INET, name, &address_b) == 1) {
			length = sizeof(struct in_addr);
			family = AF_INET;
		} else if (inet_pton(AF_INET6, name, &address_b) == 1) {
			length = sizeof(struct in6_addr);
			family = AF_INET6;
		} else {
			return -1;
		}

		ares_gethostbyaddr(IO->DNS.Channel,
				   address_b,
				   length,
				   family,
				   HostByAddrCb,
				   IO);
		EV_DNS_LOGT_START(DNS.timeout);
		ev_timer_start(event_base, &IO->DNS.timeout);

		EV_DNS_syslog(LOG_DEBUG, "C-ARES: %s X1\n", __FUNCTION__);

		return 1;

	default:

		EV_DNS_syslog(LOG_DEBUG, "C-ARES: %sX2\n", __FUNCTION__);
		return 0;
	}
	EV_DNS_syslog(LOG_DEBUG, "C-ARES: %s\n", __FUNCTION__);

	ares_query(IO->DNS.Channel, name, ns_c_in, Type, QueryCb, IO);
	EV_DNS_LOGT_START(DNS.timeout);
	ev_timer_start(event_base, &IO->DNS.timeout);
	return 1;
}





/*****************************************************************************
 *                      libev / c-ares integration                           *
 *****************************************************************************/
static void DNS_send_callback(struct ev_loop *loop, ev_io *watcher, int revents)
{
	AsyncIO *IO = watcher->data;

	IO->Now = ev_now(event_base);

	EV_DNS_syslog(LOG_DEBUG, "C-ARES: %s\n", __FUNCTION__);

	ares_process_fd(IO->DNS.Channel,
			ARES_SOCKET_BAD,
			IO->DNS.send_event.fd);
}
static void DNS_recv_callback(struct ev_loop *loop, ev_io *watcher, int revents)
{
	AsyncIO *IO = watcher->data;

	IO->Now = ev_now(event_base);

	EV_DNS_syslog(LOG_DEBUG, "C-ARES: %s\n", __FUNCTION__);

	ares_process_fd(IO->DNS.Channel,
			IO->DNS.recv_event.fd,
			ARES_SOCKET_BAD);
}

void SockStateCb(void *data, int sock, int read, int write)
{
	AsyncIO *IO = data;
/* already inside of the event queue. */
	if (DebugCAres)
	{
		struct sockaddr_in sin = {};
		socklen_t slen;
		slen = sizeof(sin);
		if ((IO->DNS.SourcePort == 0) &&
		    (getsockname(sock, &sin, &slen) == 0))
		{
			IO->DNS.SourcePort = ntohs(sin.sin_port);
		}
		EV_DNS_syslog(LOG_DEBUG, "C-ARES: %s %d|%d Sock %d port %hu\n",
			      __FUNCTION__,
			      read,
			      write,
			      sock,
			      IO->DNS.SourcePort);
	}

	IO->Now = ev_now(event_base);

	if (read) {
		if ((IO->DNS.recv_event.fd != sock) &&
		    (IO->DNS.recv_event.fd != 0)) {
			EV_DNS_LOG_STOP(DNS.recv_event);
			ev_io_stop(event_base, &IO->DNS.recv_event);
		}
		IO->DNS.recv_event.fd = sock;
		ev_io_init(&IO->DNS.recv_event,
			   DNS_recv_callback,
			   IO->DNS.recv_event.fd,
			   EV_READ);
		EV_DNS_LOG_INIT(DNS.recv_event);
		IO->DNS.recv_event.data = IO;
		EV_DNS_LOG_START(DNS.recv_event);
		ev_io_start(event_base, &IO->DNS.recv_event);
	}
	if (write) {
		if ((IO->DNS.send_event.fd != sock) &&
		    (IO->DNS.send_event.fd != 0)) {
			EV_DNS_LOG_STOP(DNS.send_event);
			ev_io_stop(event_base, &IO->DNS.send_event);
		}
		IO->DNS.send_event.fd = sock;
		ev_io_init(&IO->DNS.send_event,
			   DNS_send_callback,
			   IO->DNS.send_event.fd,
			   EV_WRITE);
		IO->DNS.send_event.data = IO;
		EV_DNS_LOG_INIT(DNS.send_event);
		EV_DNS_LOG_START(DNS.send_event);
		ev_io_start(event_base, &IO->DNS.send_event);
	}
	if ((read == 0) && (write == 0)) {
		EV_DNS_LOG_STOP(DNS.recv_event);
		EV_DNS_LOG_STOP(DNS.send_event);
		ev_io_stop(event_base, &IO->DNS.recv_event);
		ev_io_stop(event_base, &IO->DNS.send_event);
	}
}
void EnableDebugCAres(const int n)
{
	DebugCAres = n;
}

CTDL_MODULE_INIT(c_ares_client)
{
	if (!threading)
	{
		CtdlRegisterDebugFlagHook(HKEY("cares"), EnableDebugCAres, &DebugCAres);
		int r = ares_library_init(ARES_LIB_INIT_ALL);
		if (0 != r) {
			
		}
	}
	return "c-ares";
}
