/*
 * $Id$
 *
 * Citadel Dynamic Loading Module
 * Written by Brian Costello <btx@calyx.net>
 *
 */

#ifdef DLL_EXPORT
#define IN_LIBCIT
#endif

#include "sysdep.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#endif
#ifdef HAVE_DL_H
#include <dl.h>
#include "hpsux.h"
#endif
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <syslog.h>
#include <limits.h>
#include <ctype.h>
#include "citadel.h"
#include "server.h"
#include "dynloader.h"
#include "sysdep_decls.h"
#include "msgbase.h"
#include "tools.h"
#include "config.h"

#ifndef HAVE_SNPRINTF
#include <stdarg.h>
#include "snprintf.h"
#endif

struct LogFunctionHook *LogHookTable = NULL;
struct CleanupFunctionHook *CleanupHookTable = NULL;
struct SessionFunctionHook *SessionHookTable = NULL;
struct UserFunctionHook *UserHookTable = NULL;
struct XmsgFunctionHook *XmsgHookTable = NULL;
struct MessageFunctionHook *MessageHookTable = NULL;
struct NetprocFunctionHook *NetprocHookTable = NULL;
struct DeleteFunctionHook *DeleteHookTable = NULL;
struct ServiceFunctionHook *ServiceHookTable = NULL;

struct ProtoFunctionHook {
	void (*handler) (char *cmdbuf);
	char *cmd;
	char *desc;
	struct ProtoFunctionHook *next;
} *ProtoHookList = NULL;

void CtdlRegisterProtoHook(void (*handler) (char *), char *cmd, char *desc)
{
	struct ProtoFunctionHook *p;

	p = (struct ProtoFunctionHook *)
		mallok(sizeof(struct ProtoFunctionHook));

	if (p == NULL) {
		fprintf(stderr, "can't malloc new ProtoFunctionHook\n");
		exit(EXIT_FAILURE);
	}
	p->handler = handler;
	p->cmd = cmd;
	p->desc = desc;
	p->next = ProtoHookList;
	ProtoHookList = p;
	lprintf(5, "Registered server command %s (%s)\n", cmd, desc);
}


void CtdlUnregisterProtoHook(void (*handler) (char *), char *cmd)
{
	struct ProtoFunctionHook *cur, *p;

	for (cur = ProtoHookList; cur != NULL; cur = cur->next) {
		/* This will also remove duplicates if any */
		while (cur != NULL &&
				handler == cur->handler &&
				!strcmp(cmd, cur->cmd)) {
			lprintf(5, "Unregistered server command %s (%s)\n",
					cmd, cur->desc);
			p = cur->next;
			if (cur == ProtoHookList) {
				ProtoHookList = p;
			}
			phree(cur);
			cur = p;
		}
	}
}


int DLoader_Exec_Cmd(char *cmdbuf)
{
	struct ProtoFunctionHook *p;

	for (p = ProtoHookList; p; p = p->next) {
		if (!strncasecmp(cmdbuf, p->cmd, 4)) {
			p->handler(&cmdbuf[5]);
			return 1;
		}
	}
	return 0;
}

void DLoader_Init(char *pathname)
{
	void *fcn_handle;
	char dl_error[SIZ];
	DIR *dir;
	int i;
	struct dirent *dptr;
	char *(*h_init_fcn) (void);
	char *dl_info;

	char pathbuf[PATH_MAX];

	if ((dir = opendir(pathname)) == NULL) {
		perror("opendir");
		exit(1);
	}
	while ((dptr = readdir(dir)) != NULL) {
		if (strlen(dptr->d_name) < 4)
			continue;
#ifndef __CYGWIN__
		if (strcasecmp(&dptr->d_name[strlen(dptr->d_name)-3], ".so"))
#else
		if (strcasecmp(&dptr->d_name[strlen(dptr->d_name)-4], ".dll"))
#endif
			continue;

		snprintf(pathbuf, PATH_MAX, "%s/%s", pathname, dptr->d_name);
		lprintf(7, "Initializing %s...\n", pathbuf);

#ifdef RTLD_LAZY
		if (!(fcn_handle = dlopen(pathbuf, RTLD_LAZY)))
#else				/* OpenBSD */
		if (!(fcn_handle = dlopen(pathbuf, DL_LAZY)))
#endif
		{
			safestrncpy(dl_error, dlerror(), sizeof dl_error);
			for (i=0; i<strlen(dl_error); ++i)
				if (!isprint(dl_error[i]))
					dl_error[i]='.';
			fprintf(stderr, "DLoader_Init dlopen failed: %s\n",
				dl_error);
			continue;
		}
		h_init_fcn = (char * (*)(void))
#ifndef __OpenBSD__
		    dlsym(fcn_handle, "Dynamic_Module_Init");
#else
		    dlsym(fcn_handle, "_Dynamic_Module_Init");
#endif

		if (dlerror() != NULL) {
			fprintf(stderr, "DLoader_Init dlsym failed\n");
			continue;
		}
		dl_info = h_init_fcn();

		lprintf(3, "Loaded module: %s\n", dl_info);
	}	/* While */
}



void CtdlRegisterLogHook(void (*fcn_ptr) (char *), int loglevel)
{

	struct LogFunctionHook *newfcn;

	newfcn = (struct LogFunctionHook *)
	    mallok(sizeof(struct LogFunctionHook));
	newfcn->next = LogHookTable;
	newfcn->h_function_pointer = fcn_ptr;
	newfcn->loglevel = loglevel;
	LogHookTable = newfcn;

	lprintf(5, "Registered a new logging function\n");
}


void CtdlUnregisterLogHook(void (*fcn_ptr) (char *), int loglevel)
{
	struct LogFunctionHook *cur, *p;

	for (cur = LogHookTable; cur != NULL; cur = cur->next) {
		/* This will also remove duplicates if any */
		while (cur != NULL &&
				fcn_ptr == cur->h_function_pointer &&
				loglevel == cur->loglevel) {
			lprintf(5, "Unregistered logging function\n");
			p = cur->next;
			if (cur == LogHookTable) {
				LogHookTable = p;
			}
			phree(cur);
			cur = p;
		}
	}
}


void CtdlRegisterCleanupHook(void (*fcn_ptr) (void))
{

	struct CleanupFunctionHook *newfcn;

	newfcn = (struct CleanupFunctionHook *)
	    mallok(sizeof(struct CleanupFunctionHook));
	newfcn->next = CleanupHookTable;
	newfcn->h_function_pointer = fcn_ptr;
	CleanupHookTable = newfcn;

	lprintf(5, "Registered a new cleanup function\n");
}


void CtdlUnregisterCleanupHook(void (*fcn_ptr) (void))
{
	struct CleanupFunctionHook *cur, *p;

	for (cur = CleanupHookTable; cur != NULL; cur = cur->next) {
		/* This will also remove duplicates if any */
		while (cur != NULL &&
				fcn_ptr == cur->h_function_pointer) {
			lprintf(5, "Unregistered cleanup function\n");
			p = cur->next;
			if (cur == CleanupHookTable) {
				CleanupHookTable = p;
			}
			phree(cur);
			cur = p;
		}
	}
}


void CtdlRegisterSessionHook(void (*fcn_ptr) (void), int EventType)
{

	struct SessionFunctionHook *newfcn;

	newfcn = (struct SessionFunctionHook *)
	    mallok(sizeof(struct SessionFunctionHook));
	newfcn->next = SessionHookTable;
	newfcn->h_function_pointer = fcn_ptr;
	newfcn->eventtype = EventType;
	SessionHookTable = newfcn;

	lprintf(5, "Registered a new session function (type %d)\n",
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
			lprintf(5, "Unregistered session function (type %d)\n",
					EventType);
			p = cur->next;
			if (cur == SessionHookTable) {
				SessionHookTable = p;
			}
			phree(cur);
			cur = p;
		}
	}
}


void CtdlRegisterUserHook(void (*fcn_ptr) (char *, long), int EventType)
{

	struct UserFunctionHook *newfcn;

	newfcn = (struct UserFunctionHook *)
	    mallok(sizeof(struct UserFunctionHook));
	newfcn->next = UserHookTable;
	newfcn->h_function_pointer = fcn_ptr;
	newfcn->eventtype = EventType;
	UserHookTable = newfcn;

	lprintf(5, "Registered a new user function (type %d)\n",
		EventType);
}


void CtdlUnregisterUserHook(void (*fcn_ptr) (char *, long), int EventType)
{
	struct UserFunctionHook *cur, *p;

	for (cur = UserHookTable; cur != NULL; cur = cur->next) {
		/* This will also remove duplicates if any */
		while (cur != NULL &&
				fcn_ptr == cur->h_function_pointer &&
				EventType == cur->eventtype) {
			lprintf(5, "Unregistered user function (type %d)\n",
					EventType);
			p = cur->next;
			if (cur == UserHookTable) {
				UserHookTable = p;
			}
			phree(cur);
			cur = p;
		}
	}
}


void CtdlRegisterMessageHook(int (*handler)(struct CtdlMessage *),
				int EventType)
{

	struct MessageFunctionHook *newfcn;

	newfcn = (struct MessageFunctionHook *)
	    mallok(sizeof(struct MessageFunctionHook));
	newfcn->next = MessageHookTable;
	newfcn->h_function_pointer = handler;
	newfcn->eventtype = EventType;
	MessageHookTable = newfcn;

	lprintf(5, "Registered a new message function (type %d)\n",
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
			lprintf(5, "Unregistered message function (type %d)\n",
					EventType);
			p = cur->next;
			if (cur == MessageHookTable) {
				MessageHookTable = p;
			}
			phree(cur);
			cur = p;
		}
	}
}


void CtdlRegisterNetprocHook(int (*handler)(struct CtdlMessage *, char *) )
{
	struct NetprocFunctionHook *newfcn;

	newfcn = (struct NetprocFunctionHook *)
	    mallok(sizeof(struct NetprocFunctionHook));
	newfcn->next = NetprocHookTable;
	newfcn->h_function_pointer = handler;
	NetprocHookTable = newfcn;

	lprintf(5, "Registered a new netproc function\n");
}


void CtdlUnregisterNetprocHook(int (*handler)(struct CtdlMessage *, char *) )
{
	struct NetprocFunctionHook *cur, *p;

	for (cur = NetprocHookTable; cur != NULL; cur = cur->next) {
		/* This will also remove duplicates if any */
		while (cur != NULL &&
				handler == cur->h_function_pointer ) {
			lprintf(5, "Unregistered netproc function\n");
			p = cur->next;
			if (cur == NetprocHookTable) {
				NetprocHookTable = p;
			}
			phree(cur);
			cur = p;
		}
	}
}


void CtdlRegisterDeleteHook(int (*handler)(char *, long) )
{
	struct DeleteFunctionHook *newfcn;

	newfcn = (struct DeleteFunctionHook *)
	    mallok(sizeof(struct DeleteFunctionHook));
	newfcn->next = DeleteHookTable;
	newfcn->h_function_pointer = handler;
	DeleteHookTable = newfcn;

	lprintf(5, "Registered a new netproc function\n");
}


void CtdlUnregisterDeleteHook(int (*handler)(char *, long) )
{
	struct DeleteFunctionHook *cur, *p;

	for (cur = DeleteHookTable; cur != NULL; cur = cur->next) {
		/* This will also remove duplicates if any */
		while (cur != NULL &&
				handler == cur->h_function_pointer ) {
			lprintf(5, "Unregistered netproc function\n");
			p = cur->next;
			if (cur == DeleteHookTable) {
				DeleteHookTable = p;
			}
			phree(cur);
			cur = p;
		}
	}
}


void CtdlRegisterXmsgHook(int (*fcn_ptr) (char *, char *, char *), int order)
{

	struct XmsgFunctionHook *newfcn;

	newfcn = (struct XmsgFunctionHook *)
	    mallok(sizeof(struct XmsgFunctionHook));
	newfcn->next = XmsgHookTable;
	newfcn->order = order;
	newfcn->h_function_pointer = fcn_ptr;
	XmsgHookTable = newfcn;
	lprintf(5, "Registered a new x-msg function (priority %d)\n", order);
}


void CtdlUnregisterXmsgHook(int (*fcn_ptr) (char *, char *, char *), int order)
{
	struct XmsgFunctionHook *cur, *p;

	for (cur = XmsgHookTable; cur != NULL; cur = cur->next) {
		/* This will also remove duplicates if any */
		while (cur != NULL &&
				fcn_ptr == cur->h_function_pointer &&
				order == cur->order) {
			lprintf(5, "Unregistered x-msg function "
					"(priority %d)\n", order);
			p = cur->next;
			if (cur == XmsgHookTable) {
				XmsgHookTable = p;
			}
			phree(cur);
			cur = p;
		}
	}
}


void CtdlRegisterServiceHook(int tcp_port,
			char *sockpath,
			void (*h_greeting_function) (void),
			void (*h_command_function) (void) )
{
	struct ServiceFunctionHook *newfcn;
	char message[SIZ];

	newfcn = (struct ServiceFunctionHook *)
	    mallok(sizeof(struct ServiceFunctionHook));
	newfcn->next = ServiceHookTable;
	newfcn->tcp_port = tcp_port;
	newfcn->sockpath = sockpath;
	newfcn->h_greeting_function = h_greeting_function;
	newfcn->h_command_function = h_command_function;

	if (sockpath != NULL) {
		newfcn->msock = ig_uds_server(sockpath, config.c_maxsessions);
		sprintf(message, "Unix domain socket '%s': ", sockpath);
	}
	else if (tcp_port <= 0) {	/* port -1 to disable */
		lprintf(7, "Service has been manually disabled, skipping\n");
		phree(newfcn);
		return;
	}
	else {
		newfcn->msock = ig_tcp_server(tcp_port, config.c_maxsessions);
		sprintf(message, "TCP port %d: ", tcp_port);
	}

	if (newfcn->msock > 0) {
		ServiceHookTable = newfcn;
		strcat(message, "registered.");
		lprintf(5, "%s\n", message);
	}
	else {
		strcat(message, "FAILED.");
		lprintf(2, "%s\n", message);
		phree(newfcn);
	}
}


void CtdlUnregisterServiceHook(int tcp_port, char *sockpath,
			void (*h_greeting_function) (void),
			void (*h_command_function) (void) )
{
	struct ServiceFunctionHook *cur, *p;

	for (cur = ServiceHookTable; cur != NULL; cur = cur->next) {
		/* This will also remove duplicates if any */
		while (cur != NULL &&
				!(sockpath && cur->sockpath &&
					strcmp(sockpath, cur->sockpath)) &&
				h_greeting_function == cur->h_greeting_function &&
				h_command_function == cur->h_command_function &&
				tcp_port == cur->tcp_port) {
			close(cur->msock);
			if (sockpath) {
				lprintf(5, "Closed UNIX domain socket %s\n",
						sockpath);
			} else if (tcp_port) {
				lprintf(5, "Closed TCP port %d\n", tcp_port);
			} else {
				lprintf(5, "Unregistered unknown service\n");
			}
			p = cur->next;
			if (cur == ServiceHookTable) {
				ServiceHookTable = p;
			}
			phree(cur);
			cur = p;
		}
	}
}


void PerformSessionHooks(int EventType)
{
	struct SessionFunctionHook *fcn;

	for (fcn = SessionHookTable; fcn != NULL; fcn = fcn->next) {
		if (fcn->eventtype == EventType) {
			(*fcn->h_function_pointer) ();
		}
	}
}

void PerformLogHooks(int loglevel, char *logmsg)
{
	struct LogFunctionHook *fcn;

	for (fcn = LogHookTable; fcn != NULL; fcn = fcn->next) {
		if (fcn->loglevel >= loglevel) {
			(*fcn->h_function_pointer) (logmsg);
		}
	}
}

void PerformUserHooks(char *username, long usernum, int EventType)
{
	struct UserFunctionHook *fcn;

	for (fcn = UserHookTable; fcn != NULL; fcn = fcn->next) {
		if (fcn->eventtype == EventType) {
			(*fcn->h_function_pointer) (username, usernum);
		}
	}
}

int PerformMessageHooks(struct CtdlMessage *msg, int EventType)
{
	struct MessageFunctionHook *fcn;
	int total_retval = 0;

	/* Other code may elect to protect this message from server-side
	 * handlers; if this is the case, don't do anything.
	lprintf(9, "** Event type is %d, flags are %d\n",
		EventType, msg->cm_flags);
	 */
	if (msg->cm_flags & CM_SKIP_HOOKS) {
		lprintf(9, "Skipping hooks\n");
		return(0);
	}

	/* Otherwise, run all the hooks appropriate to this event type.
	 */
	for (fcn = MessageHookTable; fcn != NULL; fcn = fcn->next) {
		if (fcn->eventtype == EventType) {
			total_retval = total_retval +
				(*fcn->h_function_pointer) (msg);
		}
	}

	/* Return the sum of the return codes from the hook functions.  If
	 * this is an EVT_BEFORESAVE event, a nonzero return code will cause
	 * the save operation to abort.
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



int PerformXmsgHooks(char *sender, char *recp, char *msg)
{
	struct XmsgFunctionHook *fcn;
	int total_sent = 0;
	int p;

	for (p=0; p<MAX_XMSG_PRI; ++p) {
		for (fcn = XmsgHookTable; fcn != NULL; fcn = fcn->next) {
			if (fcn->order == p) {
				total_sent +=
					(*fcn->h_function_pointer)
						(sender, recp, msg);
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
