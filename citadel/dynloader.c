/*******************************************************
 *
 * Citadel Dynamic Loading Module
 * Written by Brian Costello
 * btx@calyx.net
 *
 * $Id$
 *
 ******************************************************/


#include "sysdep.h"
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <dirent.h>
#include <strings.h>
#include <syslog.h>
#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif
#include <limits.h>
#include <ctype.h>
#include "citadel.h"
#include "server.h"
#include "dynloader.h"
#include "sysdep_decls.h"
#include "msgbase.h"
#include "tools.h"

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

struct ProtoFunctionHook {
	void (*handler) (char *cmdbuf);
	char *cmd;
	char *desc;
	struct ProtoFunctionHook *next;
} *ProtoHookList = NULL;

void CtdlRegisterProtoHook(void (*handler) (char *), char *cmd, char *desc)
{
	struct ProtoFunctionHook *p = mallok(sizeof *p);

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
	char dl_error[256];
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
		if (dptr->d_name[0] == '.')
			continue;

		snprintf(pathbuf, PATH_MAX, "%s/%s", pathname, dptr->d_name);
#ifdef RTLD_NOW
		if (!(fcn_handle = dlopen(pathbuf, RTLD_NOW)))
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
	 */
	lprintf(9, "** Event type is %d, flags are %d\n",
		EventType, msg->cm_flags);
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
		if (total_sent) goto DONE;
	}
DONE:	return total_sent;
}
