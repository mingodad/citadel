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
struct ares_options options;
ares_channel Channel;

void SockStateCb(void *data, int sock, int read, int write);

/*
  c-ares beim connect zum nameserver:   if (channel->sock_create_cb)

  SOCK_STATE_CALLBACK(channel, s, 1, 0); -> void Channel::SockStateCb(void *data, int sock, int read, int write) {



lesen der antwort:
#0  node::Channel::QueryCb (arg=0x8579268, status=0x0, timeouts=0x0, abuf=0xbffff0ef "\356|\201\200", alen=0x48) at ../src/node_cares.cc:453
#1  0x08181884 in qcallback (arg=0x8579278, status=0x857b5a0, timeouts=0x0, abuf=0xbffff0ef "\356|\201\200", alen=0x48) at ../deps/c-ares/ares_query.c:180
#2  0x0817fbf5 in end_query (channel=<value optimized out>, query=0x85790b0, status=0x1, abuf=0xbffff0ef "\356|\201\200", alen=0x48) at ../deps/c-ares/ares_process.c:1233
#3  0x08180898 in process_answer (channel=<value optimized out>, abuf=<value optimized out>, alen=<value optimized out>, whichserver=0x0, tcp=0x0, now=0xbffff388) at ../deps/c-ares/ares_process.c:612
#4  0x08180cf8 in read_udp_packets (channel=<value optimized out>, read_fds=<value optimized out>, read_fd=<value optimized out>, now=0xbffff388) at ../deps/c-ares/ares_process.c:498
#5  0x08181021 in processfds (channel=0x85a9888, read_fds=<value optimized out>, read_fd=<value optimized out>, write_fds=0x0, write_fd=0xffffffff) at ../deps/c-ares/ares_process.c:153

-> 
static void ParseAnswerMX(QueryArg *arg, unsigned char* abuf, int alen) {
  HandleScope scope;



 */

/*

static Local<Array> HostEntToAddresses(struct hostent* hostent) {
  Local<Array> addresses = Array::New();


  char ip[INET6_ADDRSTRLEN];
  for (int i = 0; hostent->h_addr_list[i]; ++i) {
    inet_ntop(hostent->h_addrtype, hostent->h_addr_list[i], ip, sizeof(ip));

    Local<String> address = String::New(ip);
    addresses->Set(Integer::New(i), address);
  }

  return addresses;
}


static Local<Array> HostEntToNames(struct hostent* hostent) {
  Local<Array> names = Array::New();

  for (int i = 0; hostent->h_aliases[i]; ++i) {
    Local<String> address = String::New(hostent->h_aliases[i]);
    names->Set(Integer::New(i), address);
  }

  return names;
}

static inline const char *ares_errno_string(int errorno) {
#define ERRNO_CASE(e)  case ARES_##e: return #e;
  switch (errorno) {
    ERRNO_CASE(SUCCESS)
    ERRNO_CASE(ENODATA)
    ERRNO_CASE(EFORMERR)
    ERRNO_CASE(ESERVFAIL)
    ERRNO_CASE(ENOTFOUND)
    ERRNO_CASE(ENOTIMP)
    ERRNO_CASE(EREFUSED)
    ERRNO_CASE(EBADQUERY)
    ERRNO_CASE(EBADNAME)
    ERRNO_CASE(EBADFAMILY)
    ERRNO_CASE(EBADRESP)
    ERRNO_CASE(ECONNREFUSED)
    ERRNO_CASE(ETIMEOUT)
    ERRNO_CASE(EOF)
    ERRNO_CASE(EFILE)
    ERRNO_CASE(ENOMEM)
    ERRNO_CASE(EDESTRUCTION)
    ERRNO_CASE(EBADSTR)
    ERRNO_CASE(EBADFLAGS)
    ERRNO_CASE(ENONAME)
    ERRNO_CASE(EBADHINTS)
    ERRNO_CASE(ENOTINITIALIZED)
    ERRNO_CASE(ELOADIPHLPAPI)
    ERRNO_CASE(EADDRGETNETWORKPARAMS)
    ERRNO_CASE(ECANCELLED)
    default:
      assert(0 && "Unhandled c-ares errno");
      return "(UNKNOWN)";
  }
}


static void ResolveError(Persistent<Function> &cb, int status) {
  HandleScope scope;

  Local<String> code = String::NewSymbol(ares_errno_string(status));
  Local<String> message = String::NewSymbol(ares_strerror(status));

  Local<String> cons1 = String::Concat(code, String::NewSymbol(", "));
  Local<String> cons2 = String::Concat(cons1, message);

  Local<Value> e = Exception::Error(cons2);

  Local<Object> obj = e->ToObject();
  obj->Set(String::NewSymbol("errno"), Integer::New(status));

  TryCatch try_catch;

  cb->Call(v8::Context::GetCurrent()->Global(), 1, &e);

  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }
}

static void HostByNameCb(void *data,
                         int status,
                         int timeouts,
                         struct hostent *hostent) {
  HandleScope scope;

  Persistent<Function> *cb = cb_unwrap(data);

  if  (status != ARES_SUCCESS) {
    ResolveError(*cb, status);
    cb_destroy(cb);
    return;
  }

  TryCatch try_catch;

  Local<Array> addresses = HostEntToAddresses(hostent);

  Local<Value> argv[2] = { Local<Value>::New(Null()), addresses};

  (*cb)->Call(v8::Context::GetCurrent()->Global(), 2, argv);

  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }

  cb_destroy(cb);
}




static void cb_call(Persistent<Function> &cb, int argc, Local<Value> *argv) {
  TryCatch try_catch;

  cb->Call(v8::Context::GetCurrent()->Global(), argc, argv);

  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }
}

Handle<Value> Channel::GetHostByAddr(const Arguments& args) {
  HandleScope scope;
  Channel *c = ObjectWrap::Unwrap<Channel>(args.Holder());
  assert(c);

  if (!args[0]->IsString()) {
    return ThrowException(Exception::Error(
          String::New("First argument must be a address")));
  }

  if (!args[1]->IsInt32()) {
    return ThrowException(Exception::Error(
          String::New("Second argument must be an address family")));
  }

  if (!args[2]->IsFunction()) {
    return ThrowException(Exception::Error(
          String::New("Third argument must be a callback")));
  }

  int family = args[1]->Int32Value();
  if (family != AF_INET6 && family != AF_INET) {
    return ThrowException(Exception::Error(
          String::New("Unsupported address family")));
  }

  String::Utf8Value address_s(args[0]->ToString());

  char address_b[sizeof(struct in6_addr)];
  int r = inet_pton(family, *address_s, address_b);
  if (r != 1) {
    return ThrowException(Exception::Error(
          String::New("Invalid network address")));
  }

  int length;
  if (family == AF_INET6)
    length = sizeof(struct in6_addr);
  else
    length = sizeof(struct in_addr);

  ares_gethostbyaddr(c->channel, address_b, length, family, HostByAddrCb, cb_persist(args[2]));

  return Undefined();
}



Handle<Value> Channel::GetHostByName(const Arguments& args) {
  HandleScope scope;
  Channel *c = ObjectWrap::Unwrap<Channel>(args.Holder());
  assert(c);

  if (!args[0]->IsString()) {
    return ThrowException(Exception::Error(
          String::New("First argument must be a name")));
  }

  if (!args[1]->IsInt32()) {
    return ThrowException(Exception::Error(
          String::New("Second argument must be a family")));
  }

  if (!args[2]->IsFunction()) {
    return ThrowException(Exception::Error(
          String::New("Third argument must be a callback")));
  }

  int family = args[1]->Int32Value();
  if (family != AF_INET6 && family != AF_INET) {
    return ThrowException(Exception::Error(
          String::New("Unsupported address family")));
  }

  String::Utf8Value name(args[0]->ToString());

  ares_gethostbyname(c->channel, *name, family, HostByNameCb, cb_persist(args[2]));

  return Undefined();
}

*/



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
	IO->PostDNS(IO);
}

int QueueQuery(ns_type Type, char *name, AsyncIO *IO, IO_CallBack PostDNS)
{
	int length, family;
	char address_b[sizeof(struct in6_addr)];
	int optmask = 0;
	int rfd, wfd;

	optmask |= ARES_OPT_SOCK_STATE_CB;
	IO->DNSOptions.sock_state_cb = SockStateCb;
	IO->DNSOptions.sock_state_cb_data = IO;
	ares_init_options(&IO->DNSChannel, &IO->DNSOptions, optmask);

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
	ares_fds(IO->DNSChannel, &rfd, &wfd);
	return 1;
}

static void DNS_recv_callback(struct ev_loop *loop, ev_io *watcher, int revents)
{
	AsyncIO *IO = watcher->data;
	
	ares_process_fd(IO->DNSChannel, IO->sock, 0);
}

static void DNS_send_callback(struct ev_loop *loop, ev_io *watcher, int revents)
{
	AsyncIO *IO = watcher->data;
	
	ares_process_fd(IO->DNSChannel, 0, IO->sock);
}

void SockStateCb(void *data, int sock, int read, int write) 
{
	struct timeval tvbuf, maxtv, *ret;
	
	int64_t time = 10;
	AsyncIO *IO = data;
/* already inside of the event queue. */	
	IO->sock = sock;
	ev_io_init(&IO->recv_event, DNS_recv_callback, IO->sock, EV_READ);
	IO->recv_event.data = IO;
	ev_io_init(&IO->send_event, DNS_send_callback, IO->sock, EV_WRITE);
	IO->send_event.data = IO;
	if (write)
		ev_io_start(event_base, &IO->send_event);
	else
		ev_io_start(event_base, &IO->recv_event);
	

	maxtv.tv_sec = time/1000;
	maxtv.tv_usec = (time % 1000) * 1000;

	ret = ares_timeout(IO->DNSChannel, &maxtv, &tvbuf);

	
}

CTDL_MODULE_INIT(c_ares_client)
{
	if (!threading)
	{
		int optmask = 0;
		

		int r = ares_library_init(ARES_LIB_INIT_ALL);
		if (0 != r) {
			// TODO
			// ThrowException(Exception::Error(String::New(ares_strerror(r))));
////			assert(r == 0);
		}

		optmask |= ARES_OPT_SOCK_STATE_CB;
		memset(&options, 0, sizeof(struct ares_options));
		options.sock_state_cb = SockStateCb;
		
		ares_init_options(&Channel, &options, optmask);

	}
	return "c-ares";
}
