/*
 * Citadel Dynamic Loading Module
 * Written by Brian Costello <btx@calyx.net>
 *
 * Copyright (c) 1987-2011 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "sysdep.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <syslog.h>
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "serv_extensions.h"
#include "sysdep_decls.h"
#include "msgbase.h"
#include "config.h"

#include "modules/crypto/serv_crypto.h"	/* Needed until a universal crypto startup hook is implimented for CtdlStartTLS */

#include "ctdl_module.h"

#ifndef HAVE_SNPRINTF
#include <stdarg.h>
#include "snprintf.h"
#endif

struct CleanupFunctionHook *CleanupHookTable = NULL;
struct CleanupFunctionHook *EVCleanupHookTable = NULL;
struct SessionFunctionHook *SessionHookTable = NULL;
struct UserFunctionHook *UserHookTable = NULL;
struct XmsgFunctionHook *XmsgHookTable = NULL;
struct MessageFunctionHook *MessageHookTable = NULL;
struct NetprocFunctionHook *NetprocHookTable = NULL;
struct DeleteFunctionHook *DeleteHookTable = NULL;
struct ServiceFunctionHook *ServiceHookTable = NULL;
struct FixedOutputHook *FixedOutputTable = NULL;
struct RoomFunctionHook *RoomHookTable = NULL;
struct SearchFunctionHook *SearchFunctionHookTable = NULL;

struct ProtoFunctionHook {
	void (*handler) (char *cmdbuf);
	const char *cmd;
	const char *desc;
};

HashList *ProtoHookList = NULL;


#define ERR_PORT (1 << 1)


static StrBuf *portlist = NULL;

static StrBuf *errormessages = NULL;


long   DetailErrorFlags;
ConstStr Empty = {HKEY("")};
char *ErrSubject = "Startup Problems";
ConstStr ErrGeneral[] = {
	{HKEY("Citadel had trouble on starting up. ")},
	{HKEY(" This means, citadel won't be the service provider for a specific service you configured it to.\n\n"
	      "If you don't want citadel to provide these services, turn them off in WebCit via: ")},
	{HKEY("To make both ways actualy take place restart the citserver with \"sendcommand down\"\n\n"
	      "The errors returned by the system were:\n")},
	{HKEY("You can recheck the above if you follow this faq item:\n"
	      "http://www.citadel.org/doku.php/faq:mastering_your_os:net#netstat")}
};

ConstStr ErrPortShort = { HKEY("We couldn't bind all ports you configured to be provided by citadel server.\n")};
ConstStr ErrPortWhere = { HKEY("\"Admin->System Preferences->Network\".\n\nThe failed ports and sockets are: ")};
ConstStr ErrPortHint  = { HKEY("If you want citadel to provide you with that functionality, "
			       "check the output of \"netstat -lnp\" on linux Servers or \"netstat -na\" on *BSD"
			       " and stop the program that binds these ports.\n You should eventually remove "
			       " their initscripts in /etc/init.d so that you won't get this trouble once more.\n"
			       " After that goto \"Administration -> Shutdown Citadel\" to make Citadel restart & retry to bind this port.\n")};


void LogPrintMessages(long err)
{
	StrBuf *Message;
	StrBuf *List, *DetailList;
	ConstStr *Short, *Where, *Hint; 

	
	Message = NewStrBufPlain(NULL, 
				 StrLength(portlist) + StrLength(errormessages));
	
	DetailErrorFlags = DetailErrorFlags & ~err;

	switch (err)
	{
	case ERR_PORT:
		Short = &ErrPortShort;
		Where = &ErrPortWhere;
		Hint  = &ErrPortHint;
		List  = portlist;
		DetailList = errormessages;
		break;
	default:
		Short = &Empty;
		Where = &Empty;
		Hint  = &Empty;
		List  = NULL;
		DetailList = NULL;
	}

	StrBufAppendBufPlain(Message, CKEY(ErrGeneral[0]), 0);
	StrBufAppendBufPlain(Message, CKEY(*Short), 0);	
	StrBufAppendBufPlain(Message, CKEY(ErrGeneral[1]), 0);
	StrBufAppendBufPlain(Message, CKEY(*Where), 0);
	StrBufAppendBuf(Message, List, 0);
	StrBufAppendBufPlain(Message, HKEY("\n\n"), 0);
	StrBufAppendBufPlain(Message, CKEY(*Hint), 0);
	StrBufAppendBufPlain(Message, HKEY("\n\n"), 0);
	StrBufAppendBufPlain(Message, CKEY(ErrGeneral[2]), 0);
	StrBufAppendBuf(Message, DetailList, 0);
	StrBufAppendBufPlain(Message, HKEY("\n\n"), 0);
	StrBufAppendBufPlain(Message, CKEY(ErrGeneral[3]), 0);

	syslog(LOG_EMERG, "%s", ChrPtr(Message));
	syslog(LOG_EMERG, "%s", ErrSubject);
	quickie_message("Citadel", NULL, NULL, AIDEROOM, ChrPtr(Message), FMT_FIXED, ErrSubject);

	FreeStrBuf(&Message);
	FreeStrBuf(&List);
	FreeStrBuf(&DetailList);
}


void AddPortError(char *Port, char *ErrorMessage)
{
	long len;

	DetailErrorFlags |= ERR_PORT;

	len = StrLength(errormessages);
	if (len > 0) StrBufAppendBufPlain(errormessages, HKEY("; "), 0);
	else errormessages = NewStrBuf();
	StrBufAppendBufPlain(errormessages, ErrorMessage, -1, 0);


	len = StrLength(portlist);
	if (len > 0) StrBufAppendBufPlain(portlist, HKEY(";"), 0);
	else portlist = NewStrBuf();
	StrBufAppendBufPlain(portlist, Port, -1, 0);
}


int DLoader_Exec_Cmd(char *cmdbuf)
{
	void *vP;
	struct ProtoFunctionHook *p;

	if (GetHash(ProtoHookList, cmdbuf, 4, &vP) && (vP != NULL)) {
		p = (struct ProtoFunctionHook*) vP;
		p->handler(&cmdbuf[5]);
		return 1;
	}
	return 0;
}

long FourHash(const char *key, long length) 
{
	int i;
	int ret = 0;
	const unsigned char *ptr = (const unsigned char*)key;

	for (i = 0; i < 4; i++, ptr ++) 
		ret = (ret << 8) | 
			( ((*ptr >= 'a') &&
			   (*ptr <= 'z'))? 
			  *ptr - 'a' + 'A': 
			  *ptr);

	return ret;
}

void CtdlRegisterProtoHook(void (*handler) (char *), char *cmd, char *desc)
{
	struct ProtoFunctionHook *p;

	if (ProtoHookList == NULL)
		ProtoHookList = NewHash (1, FourHash);


	p = (struct ProtoFunctionHook *)
		malloc(sizeof(struct ProtoFunctionHook));

	if (p == NULL) {
		fprintf(stderr, "can't malloc new ProtoFunctionHook\n");
		exit(EXIT_FAILURE);
	}
	p->handler = handler;
	p->cmd = cmd;
	p->desc = desc;

	Put(ProtoHookList, cmd, 4, p, NULL);
	syslog(LOG_INFO, "Registered server command %s (%s)\n", cmd, desc);
}

void CtdlDestroyProtoHooks(void)
{

	DeleteHash(&ProtoHookList);
}


void CtdlRegisterCleanupHook(void (*fcn_ptr) (void))
{

	struct CleanupFunctionHook *newfcn;

	newfcn = (struct CleanupFunctionHook *)
	    malloc(sizeof(struct CleanupFunctionHook));
	newfcn->next = CleanupHookTable;
	newfcn->h_function_pointer = fcn_ptr;
	CleanupHookTable = newfcn;

	syslog(LOG_INFO, "Registered a new cleanup function\n");
}


void CtdlUnregisterCleanupHook(void (*fcn_ptr) (void))
{
	struct CleanupFunctionHook *cur, *p;

	for (cur = CleanupHookTable; cur != NULL; cur = cur->next) {
		/* This will also remove duplicates if any */
		while (cur != NULL &&
				fcn_ptr == cur->h_function_pointer) {
			syslog(LOG_INFO, "Unregistered cleanup function\n");
			p = cur->next;
			if (cur == CleanupHookTable) {
				CleanupHookTable = p;
			}
			free(cur);
			cur = p;
		}
	}
}


void CtdlDestroyCleanupHooks(void)
{
	struct CleanupFunctionHook *cur, *p;

	cur = CleanupHookTable;
	while (cur != NULL)
	{
		syslog(LOG_INFO, "Destroyed cleanup function\n");
		p = cur->next;
		free(cur);
		cur = p;
	}
	CleanupHookTable = NULL;
}

void CtdlRegisterEVCleanupHook(void (*fcn_ptr) (void))
{

	struct CleanupFunctionHook *newfcn;

	newfcn = (struct CleanupFunctionHook *)
	    malloc(sizeof(struct CleanupFunctionHook));
	newfcn->next = EVCleanupHookTable;
	newfcn->h_function_pointer = fcn_ptr;
	EVCleanupHookTable = newfcn;

	syslog(LOG_INFO, "Registered a new cleanup function\n");
}


void CtdlUnregisterEVCleanupHook(void (*fcn_ptr) (void))
{
	struct CleanupFunctionHook *cur, *p;

	for (cur = EVCleanupHookTable; cur != NULL; cur = cur->next) {
		/* This will also remove duplicates if any */
		while (cur != NULL &&
				fcn_ptr == cur->h_function_pointer) {
			syslog(LOG_INFO, "Unregistered cleanup function\n");
			p = cur->next;
			if (cur == EVCleanupHookTable) {
				EVCleanupHookTable = p;
			}
			free(cur);
			cur = p;
		}
	}
}


void CtdlDestroyEVCleanupHooks(void)
{
	struct CleanupFunctionHook *cur, *p;

	cur = EVCleanupHookTable;
	while (cur != NULL)
	{
		syslog(LOG_INFO, "Destroyed cleanup function\n");
		p = cur->next;
		free(cur);
		cur = p;
	}
	EVCleanupHookTable = NULL;
}


void CtdlRegisterSessionHook(void (*fcn_ptr) (void), int EventType)
{

	struct SessionFunctionHook *newfcn;

	newfcn = (struct SessionFunctionHook *)
	    malloc(sizeof(struct SessionFunctionHook));
	newfcn->next = SessionHookTable;
	newfcn->h_function_pointer = fcn_ptr;
	newfcn->eventtype = EventType;
	SessionHookTable = newfcn;

	syslog(LOG_INFO, "Registered a new session function (type %d)\n",
		EventType);
}


void CtdlUnregisterSessionHook(void (*fcn_ptr) (void), int EventType)
{
	struct SessionFunctionHook *cur, *p;

	for (cur = SessionHookTable; cur != NULL; cur = cur->next) {
		/* This will also remove duplicates if any */
		while (cur != NULL &&
				fcn_ptr == cur->h_function_pointer &&
				EventType == cur->eventtype) {
			syslog(LOG_INFO, "Unregistered session function (type %d)\n",
					EventType);
			p = cur->next;
			if (cur == SessionHookTable) {
				SessionHookTable = p;
			}
			free(cur);
			cur = p;
		}
	}
}

void CtdlDestroySessionHooks(void)
{
	struct SessionFunctionHook *cur, *p;

	cur = SessionHookTable;
	while (cur != NULL)
	{
		syslog(LOG_INFO, "Destroyed session function\n");
		p = cur->next;
		free(cur);
		cur = p;
	}
	SessionHookTable = NULL;
}


void CtdlRegisterUserHook(void (*fcn_ptr) (struct ctdluser *), int EventType)
{

	struct UserFunctionHook *newfcn;

	newfcn = (struct UserFunctionHook *)
	    malloc(sizeof(struct UserFunctionHook));
	newfcn->next = UserHookTable;
	newfcn->h_function_pointer = fcn_ptr;
	newfcn->eventtype = EventType;
	UserHookTable = newfcn;

	syslog(LOG_INFO, "Registered a new user function (type %d)\n",
		EventType);
}


void CtdlUnregisterUserHook(void (*fcn_ptr) (struct ctdluser *), int EventType)
{
	struct UserFunctionHook *cur, *p;

	for (cur = UserHookTable; cur != NULL; cur = cur->next) {
		/* This will also remove duplicates if any */
		while (cur != NULL &&
				fcn_ptr == cur->h_function_pointer &&
				EventType == cur->eventtype) {
			syslog(LOG_INFO, "Unregistered user function (type %d)\n",
					EventType);
			p = cur->next;
			if (cur == UserHookTable) {
				UserHookTable = p;
			}
			free(cur);
			cur = p;
		}
	}
}

void CtdlDestroyUserHooks(void)
{
	struct UserFunctionHook *cur, *p;

	cur = UserHookTable;
	while (cur != NULL)
	{
		syslog(LOG_INFO, "Destroyed user function \n");
		p = cur->next;
		free(cur);
		cur = p;
	}
	UserHookTable = NULL;
}


void CtdlRegisterMessageHook(int (*handler)(struct CtdlMessage *),
				int EventType)
{

	struct MessageFunctionHook *newfcn;

	newfcn = (struct MessageFunctionHook *)
	    malloc(sizeof(struct MessageFunctionHook));
	newfcn->next = MessageHookTable;
	newfcn->h_function_pointer = handler;
	newfcn->eventtype = EventType;
	MessageHookTable = newfcn;

	syslog(LOG_INFO, "Registered a new message function (type %d)\n",
		EventType);
}


void CtdlUnregisterMessageHook(int (*handler)(struct CtdlMessage *),
		int EventType)
{
	struct MessageFunctionHook *cur, *p;

	for (cur = MessageHookTable; cur != NULL; cur = cur->next) {
		/* This will also remove duplicates if any */
		while (cur != NULL &&
				handler == cur->h_function_pointer &&
				EventType == cur->eventtype) {
			syslog(LOG_INFO, "Unregistered message function (type %d)\n",
					EventType);
			p = cur->next;
			if (cur == MessageHookTable) {
				MessageHookTable = p;
			}
			free(cur);
			cur = p;
		}
	}
}

void CtdlDestroyMessageHook(void)
{
	struct MessageFunctionHook *cur, *p;

	cur = MessageHookTable; 
	while (cur != NULL)
	{
		syslog(LOG_INFO, "Destroyed message function (type %d)\n", cur->eventtype);
		p = cur->next;
		free(cur);
		cur = p;
	}
	MessageHookTable = NULL;
}


void CtdlRegisterRoomHook(int (*fcn_ptr)(struct ctdlroom *))
{
	struct RoomFunctionHook *newfcn;

	newfcn = (struct RoomFunctionHook *)
	    malloc(sizeof(struct RoomFunctionHook));
	newfcn->next = RoomHookTable;
	newfcn->fcn_ptr = fcn_ptr;
	RoomHookTable = newfcn;

	syslog(LOG_INFO, "Registered a new room function\n");
}


void CtdlUnregisterRoomHook(int (*fcn_ptr)(struct ctdlroom *))
{
	struct RoomFunctionHook *cur, *p;

	for (cur = RoomHookTable; cur != NULL; cur = cur->next) {
		while (cur != NULL && fcn_ptr == cur->fcn_ptr) {
			syslog(LOG_INFO, "Unregistered room function\n");
			p = cur->next;
			if (cur == RoomHookTable) {
				RoomHookTable = p;
			}
			free(cur);
			cur = p;
		}
	}
}


void CtdlDestroyRoomHooks(void)
{
	struct RoomFunctionHook *cur, *p;

	cur = RoomHookTable;
	while (cur != NULL)
	{
		syslog(LOG_INFO, "Destroyed room function\n");
		p = cur->next;
		free(cur);
		cur = p;
	}
	RoomHookTable = NULL;
}

void CtdlRegisterNetprocHook(int (*handler)(struct CtdlMessage *, char *) )
{
	struct NetprocFunctionHook *newfcn;

	newfcn = (struct NetprocFunctionHook *)
	    malloc(sizeof(struct NetprocFunctionHook));
	newfcn->next = NetprocHookTable;
	newfcn->h_function_pointer = handler;
	NetprocHookTable = newfcn;

	syslog(LOG_INFO, "Registered a new netproc function\n");
}


void CtdlUnregisterNetprocHook(int (*handler)(struct CtdlMessage *, char *) )
{
	struct NetprocFunctionHook *cur, *p;

	for (cur = NetprocHookTable; cur != NULL; cur = cur->next) {
		/* This will also remove duplicates if any */
		while (cur != NULL &&
				handler == cur->h_function_pointer ) {
			syslog(LOG_INFO, "Unregistered netproc function\n");
			p = cur->next;
			if (cur == NetprocHookTable) {
				NetprocHookTable = p;
			}
			free(cur);
			cur = p;
		}
	}
}

void CtdlDestroyNetprocHooks(void)
{
	struct NetprocFunctionHook *cur, *p;

	cur = NetprocHookTable;
	while (cur != NULL)
	{
		syslog(LOG_INFO, "Destroyed netproc function\n");
		p = cur->next;
		free(cur);
		cur = p;
	}
	NetprocHookTable = NULL;
}


void CtdlRegisterDeleteHook(void (*handler)(char *, long) )
{
	struct DeleteFunctionHook *newfcn;

	newfcn = (struct DeleteFunctionHook *)
	    malloc(sizeof(struct DeleteFunctionHook));
	newfcn->next = DeleteHookTable;
	newfcn->h_function_pointer = handler;
	DeleteHookTable = newfcn;

	syslog(LOG_INFO, "Registered a new delete function\n");
}


void CtdlUnregisterDeleteHook(void (*handler)(char *, long) )
{
	struct DeleteFunctionHook *cur, *p;

	for (cur = DeleteHookTable; cur != NULL; cur = cur->next) {
		/* This will also remove duplicates if any */
		while (cur != NULL &&
				handler == cur->h_function_pointer ) {
			syslog(LOG_INFO, "Unregistered delete function\n");
			p = cur->next;
			if (cur == DeleteHookTable) {
				DeleteHookTable = p;
			}
			free(cur);
			cur = p;
		}
	}
}
void CtdlDestroyDeleteHooks(void)
{
	struct DeleteFunctionHook *cur, *p;

	cur = DeleteHookTable;
	while (cur != NULL)
	{
		syslog(LOG_INFO, "Destroyed delete function\n");
		p = cur->next;
		free(cur);
		cur = p;		
	}
	DeleteHookTable = NULL;
}




void CtdlRegisterFixedOutputHook(char *content_type, void (*handler)(char *, int) )
{
	struct FixedOutputHook *newfcn;

	newfcn = (struct FixedOutputHook *)
	    malloc(sizeof(struct FixedOutputHook));
	newfcn->next = FixedOutputTable;
	newfcn->h_function_pointer = handler;
	safestrncpy(newfcn->content_type, content_type, sizeof newfcn->content_type);
	FixedOutputTable = newfcn;

	syslog(LOG_INFO, "Registered a new fixed output function for %s\n", newfcn->content_type);
}


void CtdlUnregisterFixedOutputHook(char *content_type)
{
	struct FixedOutputHook *cur, *p;

	for (cur = FixedOutputTable; cur != NULL; cur = cur->next) {
		/* This will also remove duplicates if any */
		while (cur != NULL && (!strcasecmp(content_type, cur->content_type))) {
			syslog(LOG_INFO, "Unregistered fixed output function for %s\n", content_type);
			p = cur->next;
			if (cur == FixedOutputTable) {
				FixedOutputTable = p;
			}
			free(cur);
			cur = p;
		}
	}
}

void CtdlDestroyFixedOutputHooks(void)
{
	struct FixedOutputHook *cur, *p;

	cur = FixedOutputTable; 
	while (cur != NULL)
	{
		syslog(LOG_INFO, "Destroyed fixed output function for %s\n", cur->content_type);
		p = cur->next;
		free(cur);
		cur = p;
		
	}
	FixedOutputTable = NULL;
}

/* returns nonzero if we found a hook and used it */
int PerformFixedOutputHooks(char *content_type, char *content, int content_length)
{
	struct FixedOutputHook *fcn;

	for (fcn = FixedOutputTable; fcn != NULL; fcn = fcn->next) {
		if (!strcasecmp(content_type, fcn->content_type)) {
			(*fcn->h_function_pointer) (content, content_length);
			return(1);
		}
	}
	return(0);
}





void CtdlRegisterXmsgHook(int (*fcn_ptr) (char *, char *, char *, char *), int order)
{

	struct XmsgFunctionHook *newfcn;

	newfcn = (struct XmsgFunctionHook *) malloc(sizeof(struct XmsgFunctionHook));
	newfcn->next = XmsgHookTable;
	newfcn->order = order;
	newfcn->h_function_pointer = fcn_ptr;
	XmsgHookTable = newfcn;
	syslog(LOG_INFO, "Registered a new x-msg function (priority %d)\n", order);
}


void CtdlUnregisterXmsgHook(int (*fcn_ptr) (char *, char *, char *, char *), int order)
{
	struct XmsgFunctionHook *cur, *p;

	for (cur = XmsgHookTable; cur != NULL; cur = cur->next) {
		/* This will also remove duplicates if any */
		while (cur != NULL &&
				fcn_ptr == cur->h_function_pointer &&
				order == cur->order) {
			syslog(LOG_INFO, "Unregistered x-msg function "
					"(priority %d)\n", order);
			p = cur->next;
			if (cur == XmsgHookTable) {
				XmsgHookTable = p;
			}
			free(cur);
			cur = p;
		}
	}
}

void CtdlDestroyXmsgHooks(void)
{
	struct XmsgFunctionHook *cur, *p;

	cur = XmsgHookTable;
	while (cur != NULL)
	{
		syslog(LOG_INFO, "Destroyed x-msg function "
			"(priority %d)\n", cur->order);
		p = cur->next;
			
		free(cur);
		cur = p;
	}
	XmsgHookTable = NULL;
}


void CtdlRegisterServiceHook(int tcp_port,
			     char *sockpath,
			     void (*h_greeting_function) (void),
			     void (*h_command_function) (void),
			     void (*h_async_function) (void),
			     const char *ServiceName)
{
	struct ServiceFunctionHook *newfcn;
	char *message;
	char error[SIZ];

	strcpy(error, "");
	newfcn = (struct ServiceFunctionHook *) malloc(sizeof(struct ServiceFunctionHook));
	message = (char*) malloc (SIZ + SIZ);
	
	newfcn->next = ServiceHookTable;
	newfcn->tcp_port = tcp_port;
	newfcn->sockpath = sockpath;
	newfcn->h_greeting_function = h_greeting_function;
	newfcn->h_command_function = h_command_function;
	newfcn->h_async_function = h_async_function;
	newfcn->ServiceName = ServiceName;

	if (sockpath != NULL) {
		newfcn->msock = ctdl_uds_server(sockpath, config.c_maxsessions, error);
		snprintf(message, SIZ, "Unix domain socket '%s': ", sockpath);
	}
	else if (tcp_port <= 0) {	/* port -1 to disable */
		syslog(LOG_INFO, "Service %s has been manually disabled, skipping\n", ServiceName);
		free (message);
		free(newfcn);
		return;
	}
	else {
		newfcn->msock = ctdl_tcp_server(config.c_ip_addr,
					      tcp_port,
					      config.c_maxsessions, 
					      error);
		snprintf(message, SIZ, "TCP port %s:%d: (%s) ", 
			 config.c_ip_addr, tcp_port, ServiceName);
	}

	if (newfcn->msock > 0) {
		ServiceHookTable = newfcn;
		strcat(message, "registered.");
		syslog(LOG_INFO, "%s\n", message);
	}
	else {
		AddPortError(message, error);
		strcat(message, "FAILED.");
		syslog(LOG_CRIT, "%s\n", message);
		free(newfcn);
	}
	free(message);
}


void CtdlUnregisterServiceHook(int tcp_port, char *sockpath,
			void (*h_greeting_function) (void),
			void (*h_command_function) (void),
			void (*h_async_function) (void)
			)
{
	struct ServiceFunctionHook *cur, *p;

	cur = ServiceHookTable;
	while (cur != NULL) {
		/* This will also remove duplicates if any */
		while (cur != NULL &&
				!(sockpath && cur->sockpath &&
					strcmp(sockpath, cur->sockpath)) &&
				h_greeting_function == cur->h_greeting_function &&
				h_command_function == cur->h_command_function &&
				h_async_function == cur->h_async_function &&
				tcp_port == cur->tcp_port) {
			close(cur->msock);
			if (sockpath) {
				syslog(LOG_INFO, "Closed UNIX domain socket %s\n",
						sockpath);
			} else if (tcp_port) {
				syslog(LOG_INFO, "Closed TCP port %d\n", tcp_port);
			} else {
				syslog(LOG_INFO, "Unregistered service \"%s\"\n", cur->ServiceName);
			}
			p = cur->next;
			if (cur == ServiceHookTable) {
				ServiceHookTable = p;
			}
			free(cur);
			cur = p;
		}
	}
}


void CtdlShutdownServiceHooks(void)
{
	/* sort of a duplicate of close_masters() but called earlier */
	struct ServiceFunctionHook *cur;

	cur = ServiceHookTable;
	while (cur != NULL) 
	{
		if (cur->msock != -1)
		{
			close(cur->msock);
			cur->msock = -1;
			if (cur->sockpath != NULL){
				syslog(LOG_INFO, "[%s] Closed UNIX domain socket %s\n",
					      cur->ServiceName,
					      cur->sockpath);
			} else {
				syslog(LOG_INFO, "[%s] closing service\n", 
					      cur->ServiceName);
			}
		}
		cur = cur->next;
	}
}

void CtdlDestroyServiceHook(void)
{
	struct ServiceFunctionHook *cur, *p;

	cur = ServiceHookTable;
	while (cur != NULL)
	{
		close(cur->msock);
		if (cur->sockpath) {
			syslog(LOG_INFO, "Closed UNIX domain socket %s\n",
				cur->sockpath);
		} else if (cur->tcp_port) {
			syslog(LOG_INFO, "Closed TCP port %d\n", cur->tcp_port);
		} else {
			syslog(LOG_INFO, "Destroyed service \"%s\"\n", cur->ServiceName);
		}
		p = cur->next;
		free(cur);
		cur = p;
	}
	ServiceHookTable = NULL;
}

void CtdlRegisterSearchFuncHook(void (*fcn_ptr)(int *, long **, const char *), char *name)
{
	struct SearchFunctionHook *newfcn;

	if (!name || !fcn_ptr) {
		return;
	}
	
	newfcn = (struct SearchFunctionHook *)
	    malloc(sizeof(struct SearchFunctionHook));
	newfcn->next = SearchFunctionHookTable;
	newfcn->name = name;
	newfcn->fcn_ptr = fcn_ptr;
	SearchFunctionHookTable = newfcn;

	syslog(LOG_INFO, "Registered a new search function (%s)\n", name);
}

void CtdlUnregisterSearchFuncHook(void (*fcn_ptr)(int *, long **, const char *), char *name)
{
	struct SearchFunctionHook *cur, *p;
	
	for (cur = SearchFunctionHookTable; cur != NULL; cur = cur->next) {
		while (fcn_ptr && (cur->fcn_ptr == fcn_ptr) && name && !strcmp(name, cur->name)) {
			syslog(LOG_INFO, "Unregistered search function(%s)\n", name);
			p = cur->next;
			if (cur == SearchFunctionHookTable) {
				SearchFunctionHookTable = p;
			}
			free (cur);
			cur = p;
		}
	}
}

void CtdlDestroySearchHooks(void)
{
        struct SearchFunctionHook *cur, *p;

	cur = SearchFunctionHookTable;
	SearchFunctionHookTable = NULL;
        while (cur != NULL) {
		p = cur->next;
		free(cur);
		cur = p;
	}
}

void CtdlModuleDoSearch(int *num_msgs, long **search_msgs, const char *search_string, const char *func_name)
{
	struct SearchFunctionHook *fcn = NULL;

	for (fcn = SearchFunctionHookTable; fcn != NULL; fcn = fcn->next) {
		if (!func_name || !strcmp(func_name, fcn->name)) {
			(*fcn->fcn_ptr) (num_msgs, search_msgs, search_string);
			return;
		}
	}
	*num_msgs = 0;
}


void PerformSessionHooks(int EventType)
{
	struct SessionFunctionHook *fcn = NULL;

	for (fcn = SessionHookTable; fcn != NULL; fcn = fcn->next) {
		if (fcn->eventtype == EventType) {
			if (EventType == EVT_TIMER) {
				pthread_setspecific(MyConKey, NULL);	/* for every hook */
			}
			(*fcn->h_function_pointer) ();
		}
	}
}

void PerformUserHooks(struct ctdluser *usbuf, int EventType)
{
	struct UserFunctionHook *fcn = NULL;

	for (fcn = UserHookTable; fcn != NULL; fcn = fcn->next) {
		if (fcn->eventtype == EventType) {
			(*fcn->h_function_pointer) (usbuf);
		}
	}
}

int PerformMessageHooks(struct CtdlMessage *msg, int EventType)
{
	struct MessageFunctionHook *fcn = NULL;
	int total_retval = 0;

	/* Other code may elect to protect this message from server-side
	 * handlers; if this is the case, don't do anything.
	syslog(LOG_DEBUG, "** Event type is %d, flags are %d\n", EventType, msg->cm_flags);
	 */
	if (msg->cm_flags & CM_SKIP_HOOKS) {
		syslog(LOG_DEBUG, "Skipping hooks\n");
		return(0);
	}

	/* Otherwise, run all the hooks appropriate to this event type.
	 */
	for (fcn = MessageHookTable; fcn != NULL; fcn = fcn->next) {
		if (fcn->eventtype == EventType) {
			total_retval = total_retval + (*fcn->h_function_pointer) (msg);
		}
	}

	/* Return the sum of the return codes from the hook functions.  If
	 * this is an EVT_BEFORESAVE event, a nonzero return code will cause
	 * the save operation to abort.
	 */
	return total_retval;
}


int PerformRoomHooks(struct ctdlroom *target_room)
{
	struct RoomFunctionHook *fcn;
	int total_retval = 0;

	syslog(LOG_DEBUG, "Performing room hooks for <%s>\n", target_room->QRname);

	for (fcn = RoomHookTable; fcn != NULL; fcn = fcn->next) {
		total_retval = total_retval + (*fcn->fcn_ptr) (target_room);
	}

	/* Return the sum of the return codes from the hook functions.
	 */
	return total_retval;
}


int PerformNetprocHooks(struct CtdlMessage *msg, char *target_room)
{
	struct NetprocFunctionHook *fcn;
	int total_retval = 0;

	for (fcn = NetprocHookTable; fcn != NULL; fcn = fcn->next) {
		total_retval = total_retval +
			(*fcn->h_function_pointer) (msg, target_room);
	}

	/* Return the sum of the return codes from the hook functions.
	 * A nonzero return code will cause the message to *not* be imported.
	 */
	return total_retval;
}


void PerformDeleteHooks(char *room, long msgnum)
{
	struct DeleteFunctionHook *fcn;

	for (fcn = DeleteHookTable; fcn != NULL; fcn = fcn->next) {
		(*fcn->h_function_pointer) (room, msgnum);
	}
}





int PerformXmsgHooks(char *sender, char *sender_email, char *recp, char *msg)
{
	struct XmsgFunctionHook *fcn;
	int total_sent = 0;
	int p;

	for (p=0; p<MAX_XMSG_PRI; ++p) {
		for (fcn = XmsgHookTable; fcn != NULL; fcn = fcn->next) {
			if (fcn->order == p) {
				total_sent +=
					(*fcn->h_function_pointer)
						(sender, sender_email, recp, msg);
			}
		}
		/* Break out of the loop if a higher-priority function
		 * successfully delivered the message.  This prevents duplicate
		 * deliveries to local users simultaneously signed onto
		 * remote services.
		 */
		if (total_sent) break;
	}
	return total_sent;
}


/*
 * Dirty hack until we impliment a hook mechanism for this
 */
void CtdlModuleStartCryptoMsgs(char *ok_response, char *nosup_response, char *error_response)
{
#ifdef HAVE_OPENSSL
	CtdlStartTLS (ok_response, nosup_response, error_response);
#endif
}
