/*
 * $Id$
 *
 * Citadel Dynamic Loading Module
 * Written by Brian Costello <btx@calyx.net>
 *
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
#include "citadel.h"
#include "server.h"
#include "serv_extensions.h"
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
		malloc(sizeof(struct ProtoFunctionHook));

	if (p == NULL) {
		fprintf(stderr, "can't malloc new ProtoFunctionHook\n");
		exit(EXIT_FAILURE);
	}
	p->handler = handler;
	p->cmd = cmd;
	p->desc = desc;
	p->next = ProtoHookList;
	ProtoHookList = p;
	lprintf(CTDL_INFO, "Registered server command %s (%s)\n", cmd, desc);
}


void CtdlUnregisterProtoHook(void (*handler) (char *), char *cmd)
{
	struct ProtoFunctionHook *cur, *p;

	for (cur = ProtoHookList; cur != NULL; cur = cur->next) {
		/* This will also remove duplicates if any */
		while (cur != NULL &&
				handler == cur->handler &&
				!strcmp(cmd, cur->cmd)) {
			lprintf(CTDL_INFO, "Unregistered server command %s (%s)\n",
					cmd, cur->desc);
			p = cur->next;
			if (cur == ProtoHookList) {
				ProtoHookList = p;
			}
			free(cur);
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

void initialize_server_extensions(void)
{
	serv_bio_init();
	serv_calendar_init();
	serv_ldap_init();
	serv_chat_init();
	serv_expire_init();
	serv_imap_init();
	serv_inetcfg_init();
	serv_listsub_init();
	serv_mrtg_init();
	serv_netfilter_init();
	serv_network_init();
	serv_newuser_init();
	serv_pas2_init();
	serv_pop3_init();
	serv_rwho_init();
	serv_smtp_init();
	serv_spam_init();
	/* serv_test_init(); */
	serv_upgrade_init();
	serv_vandelay_init();
	serv_vcard_init();
}



void CtdlRegisterLogHook(void (*fcn_ptr) (char *), int loglevel)
{

	struct LogFunctionHook *newfcn;

	newfcn = (struct LogFunctionHook *)
	    malloc(sizeof(struct LogFunctionHook));
	newfcn->next = LogHookTable;
	newfcn->h_function_pointer = fcn_ptr;
	newfcn->loglevel = loglevel;
	LogHookTable = newfcn;

	lprintf(CTDL_INFO, "Registered a new logging function\n");
}


void CtdlUnregisterLogHook(void (*fcn_ptr) (char *), int loglevel)
{
	struct LogFunctionHook *cur, *p;

	for (cur = LogHookTable; cur != NULL; cur = cur->next) {
		/* This will also remove duplicates if any */
		while (cur != NULL &&
				fcn_ptr == cur->h_function_pointer &&
				loglevel == cur->loglevel) {
			lprintf(CTDL_INFO, "Unregistered logging function\n");
			p = cur->next;
			if (cur == LogHookTable) {
				LogHookTable = p;
			}
			free(cur);
			cur = p;
		}
	}
}


void CtdlRegisterCleanupHook(void (*fcn_ptr) (void))
{

	struct CleanupFunctionHook *newfcn;

	newfcn = (struct CleanupFunctionHook *)
	    malloc(sizeof(struct CleanupFunctionHook));
	newfcn->next = CleanupHookTable;
	newfcn->h_function_pointer = fcn_ptr;
	CleanupHookTable = newfcn;

	lprintf(CTDL_INFO, "Registered a new cleanup function\n");
}


void CtdlUnregisterCleanupHook(void (*fcn_ptr) (void))
{
	struct CleanupFunctionHook *cur, *p;

	for (cur = CleanupHookTable; cur != NULL; cur = cur->next) {
		/* This will also remove duplicates if any */
		while (cur != NULL &&
				fcn_ptr == cur->h_function_pointer) {
			lprintf(CTDL_INFO, "Unregistered cleanup function\n");
			p = cur->next;
			if (cur == CleanupHookTable) {
				CleanupHookTable = p;
			}
			free(cur);
			cur = p;
		}
	}
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

	lprintf(CTDL_INFO, "Registered a new session function (type %d)\n",
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
			lprintf(CTDL_INFO, "Unregistered session function (type %d)\n",
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


void CtdlRegisterUserHook(void (*fcn_ptr) (struct ctdluser *), int EventType)
{

	struct UserFunctionHook *newfcn;

	newfcn = (struct UserFunctionHook *)
	    malloc(sizeof(struct UserFunctionHook));
	newfcn->next = UserHookTable;
	newfcn->h_function_pointer = fcn_ptr;
	newfcn->eventtype = EventType;
	UserHookTable = newfcn;

	lprintf(CTDL_INFO, "Registered a new user function (type %d)\n",
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
			lprintf(CTDL_INFO, "Unregistered user function (type %d)\n",
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

	lprintf(CTDL_INFO, "Registered a new message function (type %d)\n",
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
			lprintf(CTDL_INFO, "Unregistered message function (type %d)\n",
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


void CtdlRegisterNetprocHook(int (*handler)(struct CtdlMessage *, char *) )
{
	struct NetprocFunctionHook *newfcn;

	newfcn = (struct NetprocFunctionHook *)
	    malloc(sizeof(struct NetprocFunctionHook));
	newfcn->next = NetprocHookTable;
	newfcn->h_function_pointer = handler;
	NetprocHookTable = newfcn;

	lprintf(CTDL_INFO, "Registered a new netproc function\n");
}


void CtdlUnregisterNetprocHook(int (*handler)(struct CtdlMessage *, char *) )
{
	struct NetprocFunctionHook *cur, *p;

	for (cur = NetprocHookTable; cur != NULL; cur = cur->next) {
		/* This will also remove duplicates if any */
		while (cur != NULL &&
				handler == cur->h_function_pointer ) {
			lprintf(CTDL_INFO, "Unregistered netproc function\n");
			p = cur->next;
			if (cur == NetprocHookTable) {
				NetprocHookTable = p;
			}
			free(cur);
			cur = p;
		}
	}
}


void CtdlRegisterDeleteHook(void (*handler)(char *, long) )
{
	struct DeleteFunctionHook *newfcn;

	newfcn = (struct DeleteFunctionHook *)
	    malloc(sizeof(struct DeleteFunctionHook));
	newfcn->next = DeleteHookTable;
	newfcn->h_function_pointer = handler;
	DeleteHookTable = newfcn;

	lprintf(CTDL_INFO, "Registered a new netproc function\n");
}


void CtdlUnregisterDeleteHook(void (*handler)(char *, long) )
{
	struct DeleteFunctionHook *cur, *p;

	for (cur = DeleteHookTable; cur != NULL; cur = cur->next) {
		/* This will also remove duplicates if any */
		while (cur != NULL &&
				handler == cur->h_function_pointer ) {
			lprintf(CTDL_INFO, "Unregistered netproc function\n");
			p = cur->next;
			if (cur == DeleteHookTable) {
				DeleteHookTable = p;
			}
			free(cur);
			cur = p;
		}
	}
}


void CtdlRegisterXmsgHook(int (*fcn_ptr) (char *, char *, char *), int order)
{

	struct XmsgFunctionHook *newfcn;

	newfcn = (struct XmsgFunctionHook *)
	    malloc(sizeof(struct XmsgFunctionHook));
	newfcn->next = XmsgHookTable;
	newfcn->order = order;
	newfcn->h_function_pointer = fcn_ptr;
	XmsgHookTable = newfcn;
	lprintf(CTDL_INFO, "Registered a new x-msg function (priority %d)\n", order);
}


void CtdlUnregisterXmsgHook(int (*fcn_ptr) (char *, char *, char *), int order)
{
	struct XmsgFunctionHook *cur, *p;

	for (cur = XmsgHookTable; cur != NULL; cur = cur->next) {
		/* This will also remove duplicates if any */
		while (cur != NULL &&
				fcn_ptr == cur->h_function_pointer &&
				order == cur->order) {
			lprintf(CTDL_INFO, "Unregistered x-msg function "
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


void CtdlRegisterServiceHook(int tcp_port,
			char *sockpath,
			void (*h_greeting_function) (void),
			void (*h_command_function) (void),
			void (*h_async_function) (void)
			)
{
	struct ServiceFunctionHook *newfcn;
	char message[SIZ];

	newfcn = (struct ServiceFunctionHook *)
	    malloc(sizeof(struct ServiceFunctionHook));
	newfcn->next = ServiceHookTable;
	newfcn->tcp_port = tcp_port;
	newfcn->sockpath = sockpath;
	newfcn->h_greeting_function = h_greeting_function;
	newfcn->h_command_function = h_command_function;
	newfcn->h_async_function = h_async_function;

	if (sockpath != NULL) {
		newfcn->msock = ig_uds_server(sockpath, config.c_maxsessions);
		snprintf(message, sizeof message, "Unix domain socket '%s': ", sockpath);
	}
	else if (tcp_port <= 0) {	/* port -1 to disable */
		lprintf(CTDL_INFO, "Service has been manually disabled, skipping\n");
		free(newfcn);
		return;
	}
	else {
		newfcn->msock = ig_tcp_server(NULL, tcp_port,
					config.c_maxsessions);
		snprintf(message, sizeof message, "TCP port %d: ", tcp_port);
	}

	if (newfcn->msock > 0) {
		ServiceHookTable = newfcn;
		strcat(message, "registered.");
		lprintf(CTDL_INFO, "%s\n", message);
	}
	else {
		strcat(message, "FAILED.");
		lprintf(CTDL_CRIT, "%s\n", message);
		free(newfcn);
	}
}


void CtdlUnregisterServiceHook(int tcp_port, char *sockpath,
			void (*h_greeting_function) (void),
			void (*h_command_function) (void),
			void (*h_async_function) (void)
			)
{
	struct ServiceFunctionHook *cur, *p;

	for (cur = ServiceHookTable; cur != NULL; cur = cur->next) {
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
				lprintf(CTDL_INFO, "Closed UNIX domain socket %s\n",
						sockpath);
			} else if (tcp_port) {
				lprintf(CTDL_INFO, "Closed TCP port %d\n", tcp_port);
			} else {
				lprintf(CTDL_INFO, "Unregistered unknown service\n");
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

void PerformUserHooks(struct ctdluser *usbuf, int EventType)
{
	struct UserFunctionHook *fcn;

	for (fcn = UserHookTable; fcn != NULL; fcn = fcn->next) {
		if (fcn->eventtype == EventType) {
			(*fcn->h_function_pointer) (usbuf);
		}
	}
}

int PerformMessageHooks(struct CtdlMessage *msg, int EventType)
{
	struct MessageFunctionHook *fcn;
	int total_retval = 0;

	/* Other code may elect to protect this message from server-side
	 * handlers; if this is the case, don't do anything.
	lprintf(CTDL_DEBUG, "** Event type is %d, flags are %d\n",
		EventType, msg->cm_flags);
	 */
	if (msg->cm_flags & CM_SKIP_HOOKS) {
		lprintf(CTDL_DEBUG, "Skipping hooks\n");
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
