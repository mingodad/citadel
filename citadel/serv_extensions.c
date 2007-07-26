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

struct CleanupFunctionHook *CleanupHookTable = NULL;
struct SessionFunctionHook *SessionHookTable = NULL;
struct UserFunctionHook *UserHookTable = NULL;
struct XmsgFunctionHook *XmsgHookTable = NULL;
struct MessageFunctionHook *MessageHookTable = NULL;
struct NetprocFunctionHook *NetprocHookTable = NULL;
struct DeleteFunctionHook *DeleteHookTable = NULL;
struct ServiceFunctionHook *ServiceHookTable = NULL;
struct FixedOutputHook *FixedOutputTable = NULL;
struct RoomFunctionHook *RoomHookTable = NULL;


struct ProtoFunctionHook {
	void (*handler) (char *cmdbuf);
	char *cmd;
	char *desc;
	struct ProtoFunctionHook *next;
} *ProtoHookList = NULL;


#define ERR_PORT (1 << 1)


static char *portlist = NULL;
static size_t nSizPort = 0;

static char *errormessages = NULL;
static size_t nSizErrmsg = 0;


static long   DetailErrorFlags;

char *ErrSubject = "Startup Problems";
char *ErrGeneral = "Citadel had trouble on starting up. %s This means, citadel won't be the service provider for a specific service you configured it to.\n\n"
"If you don't want citadel to provide these services, turn them off in WebCit via %s%s\n\n%s\n\n"
"To make both ways actualy take place restart the citserver with \"sendcommand down\"\n\n"
"The errors returned by the system were:\n%s\n"
"You can recheck the above if you follow this faq item:\n"
"http://www.citadel.org/doku.php/faq:mastering_your_os:net#netstat";


char *ErrPortShort = "We couldn't bind all ports you configured to be provided by citadel server.";
char *ErrPortWhere = "Admin->System Preferences->Network.\n\nThe failed ports and sockets are: ";
char *ErrPortHint = "If you want citadel to provide you with that functionality, "
"check the output of \"netstat -lnp\" on linux Servers or \"netstat -na\" on *BSD"
" and stop the programm, that binds these ports. You should eventually remove "
" their initscripts in /etc/init.d so that you won't get this trouble once more.\n";


void LogPrintMessages(long err)
{
	char *List, *DetailList, *Short, *Where, *Hint, *Message; 
	int n = nSizPort + nSizErrmsg + 5;

	Message = (char*) malloc(n * SIZ);

	switch (err)
	{
	case ERR_PORT:
		Short = ErrPortShort;
		Where = ErrPortWhere;
		Hint  = ErrPortHint;
		List  = portlist;
		DetailList = errormessages;
		break;
	default:
		Short = "";
		Where = "";
		Hint  = "";
		List  = "";
		DetailList = "";
	}


	snprintf(Message, n * SIZ, ErrGeneral, Short, Where, List, Hint, DetailList);

	quickie_message("Citadel", NULL, NULL, AIDEROOM, Message, FMT_FIXED, ErrSubject);
	if (errormessages!=NULL) free (errormessages);
	errormessages = NULL;
	if (portlist!=NULL) free (portlist);
	portlist = NULL;
	free(Message);
}



void AppendString(char **target, char *append, size_t *len, size_t rate)
{
	size_t oLen = 0;
	long AddLen;
	long RelPtr = 0;

	AddLen = strlen(append);

	if (*len == 0)
	{
		*len = rate;

		*target = (char*)malloc (*len * SIZ);
	}
	else 
	{
		oLen = strlen(*target);
		RelPtr = strlen(*target);
		if (oLen + AddLen + 2 > *len * SIZ)
		{
			char *Buff = *target;
			size_t NewSiz = *len + 10;
			*target = malloc (NewSiz * SIZ);
			memcpy (*target, Buff, NewSiz * SIZ);
			*len = NewSiz;
		}
	}
	memcpy (*target + oLen, append, AddLen);
	(*target)[oLen + AddLen + 1] = '\n';
	(*target)[oLen + AddLen + 2] = '\0';
}

void AddPortError(char *Port, char *ErrorMessage)
{
	char *pos;
	long len;

	DetailErrorFlags |= ERR_PORT;

	AppendString(&errormessages, ErrorMessage, &nSizErrmsg, 10);
	AppendString(&portlist, Port, &nSizPort, 2);

	pos = strchr (portlist, ':');
	if (pos != NULL) *pos = ';';
	
	len = strlen (errormessages);
	if (nSizErrmsg * SIZ > len + 3)
	{
		errormessages[len] = ';';
		errormessages[len+1] = ' ';
		errormessages[len+2] = '\0';
	}	
}


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
	struct ProtoFunctionHook *cur = NULL;
	struct ProtoFunctionHook *p = NULL;
	struct ProtoFunctionHook *lastcur = NULL;

	for (cur = ProtoHookList; 
	     cur != NULL; 
	     cur = (cur != NULL)? cur->next: NULL) {
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
			else if (lastcur != NULL)
			{
				lastcur->next = p;
			}
			free(cur);
			cur = p;
		}
		lastcur = cur;
	}
}

void CtdlDestroyProtoHooks(void)
{
	struct ProtoFunctionHook *cur, *p;

	cur = ProtoHookList; 
	while (cur != NULL)
	{
		lprintf(CTDL_INFO, "Destroyed server command %s (%s)\n",
			cur->cmd, cur->desc);
		p = cur->next;
		free(cur);
		cur = p;
	}
	ProtoHookList = NULL;
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

#if 0
void initialize_server_extensions(void)
{
	long filter;

	nSizErrmsg = 0;

	/*lprintf(CTDL_INFO, "%s\n", serv_bio_init());
	lprintf(CTDL_INFO, "%s\n", serv_calendar_init());
	lprintf(CTDL_INFO, "%s\n", serv_notes_init());
	lprintf(CTDL_INFO, "%s\n", serv_ldap_init());
	lprintf(CTDL_INFO, "%s\n", serv_chat_init());
	lprintf(CTDL_INFO, "%s\n", serv_expire_init());
	lprintf(CTDL_INFO, "%s\n", serv_imap_init());
	lprintf(CTDL_INFO, "%s\n", serv_upgrade_init());
	lprintf(CTDL_INFO, "%s\n", serv_inetcfg_init());
	lprintf(CTDL_INFO, "%s\n", serv_listsub_init());
	lprintf(CTDL_INFO, "%s\n", serv_mrtg_init());
	lprintf(CTDL_INFO, "%s\n", serv_netfilter_init());
	lprintf(CTDL_INFO, "%s\n", serv_network_init());
	lprintf(CTDL_INFO, "%s\n", serv_newuser_init());
	lprintf(CTDL_INFO, "%s\n", serv_pas2_init());
	lprintf(CTDL_INFO, "%s\n", serv_smtp_init());
	lprintf(CTDL_INFO, "%s\n", serv_pop3_init());
	lprintf(CTDL_INFO, "%s\n", serv_rwho_init());
	lprintf(CTDL_INFO, "%s\n", serv_spam_init());*/
	/* lprintf(CTDL_INFO, "%s\n", serv_test_init()); */
	/*lprintf(CTDL_INFO, "%s\n", serv_vandelay_init());
	lprintf(CTDL_INFO, "%s\n", serv_vcard_init());
	lprintf(CTDL_INFO, "%s\n", serv_fulltext_init());
	lprintf(CTDL_INFO, "%s\n", serv_autocompletion_init());
	lprintf(CTDL_INFO, "%s\n", serv_postfix_tcpdict());
	lprintf(CTDL_INFO, "%s\n", serv_sieve_init());
	lprintf(CTDL_INFO, "%s\n", serv_managesieve_init());
	lprintf(CTDL_INFO, "%s\n", serv_funambol_init());*/
	for (filter = 1; filter != 0; filter = filter << 1)
		if ((filter & DetailErrorFlags) != 0)
			LogPrintMessages(filter);
}

#endif

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

void CtdlDestroyCleanupHooks(void)
{
	struct CleanupFunctionHook *cur, *p;

	cur = CleanupHookTable;
	while (cur != NULL)
	{
		lprintf(CTDL_INFO, "Destroyed cleanup function\n");
		p = cur->next;
		free(cur);
		cur = p;
	}
	CleanupHookTable = NULL;
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

void CtdlDestroySessionHooks(void)
{
	struct SessionFunctionHook *cur, *p;

	cur = SessionHookTable;
	while (cur != NULL)
	{
		lprintf(CTDL_INFO, "Destroyed session function\n");
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

void CtdlDestroyUserHooks(void)
{
	struct UserFunctionHook *cur, *p;

	cur = UserHookTable;
	while (cur != NULL)
	{
		lprintf(CTDL_INFO, "Destroyed user function \n");
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

void CtdlDestroyMessageHook(void)
{
	struct MessageFunctionHook *cur, *p;

	cur = MessageHookTable; 
	while (cur != NULL)
	{
		lprintf(CTDL_INFO, "Destroyed message function \n");
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

	lprintf(CTDL_INFO, "Registered a new room function\n");
}


void CtdlUnregisterRoomHook(int (*fcn_ptr)(struct ctdlroom *))
{
	struct RoomFunctionHook *cur, *p;

	for (cur = RoomHookTable; cur != NULL; cur = cur->next) {
		while (cur != NULL && fcn_ptr == cur->fcn_ptr) {
			lprintf(CTDL_INFO, "Unregistered room function\n");
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
		lprintf(CTDL_INFO, "Unregistered room function\n");
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

void CtdlDestroyNetprocHooks(void)
{
	struct NetprocFunctionHook *cur, *p;

	cur = NetprocHookTable;
	while (cur != NULL)
	{
		lprintf(CTDL_INFO, "Unregistered netproc function\n");
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
void CtdlDestroyDeleteHooks(void)
{
	struct DeleteFunctionHook *cur, *p;

	cur = DeleteHookTable;
	while (cur != NULL)
	{
		lprintf(CTDL_INFO, "Destroyed netproc function\n");
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

	lprintf(CTDL_INFO, "Registered a new fixed output function for %s\n", newfcn->content_type);
}


void CtdlUnregisterFixedOutputHook(char *content_type)
{
	struct FixedOutputHook *cur, *p;

	for (cur = FixedOutputTable; cur != NULL; cur = cur->next) {
		/* This will also remove duplicates if any */
		while (cur != NULL && (!strcasecmp(content_type, cur->content_type))) {
			lprintf(CTDL_INFO, "Unregistered fixed output function for %s\n", content_type);
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
		lprintf(CTDL_INFO, "Destroyed fixed output function for %s\n", cur->content_type);
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

void CtdlDestroyXmsgHooks(void)
{
	struct XmsgFunctionHook *cur, *p;

	cur = XmsgHookTable;
	while (cur != NULL)
	{
		lprintf(CTDL_INFO, "Destroyed x-msg function "
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
			void (*h_async_function) (void)
			)
{
	struct ServiceFunctionHook *newfcn;
	char *message;
	char *error;

	error = NULL;
	newfcn = (struct ServiceFunctionHook *)
	    malloc(sizeof(struct ServiceFunctionHook));
	message = (char*) malloc (SIZ);
	
	newfcn->next = ServiceHookTable;
	newfcn->tcp_port = tcp_port;
	newfcn->sockpath = sockpath;
	newfcn->h_greeting_function = h_greeting_function;
	newfcn->h_command_function = h_command_function;
	newfcn->h_async_function = h_async_function;

	if (sockpath != NULL) {
		newfcn->msock = ig_uds_server(sockpath, config.c_maxsessions, &error);
		snprintf(message, SIZ, "Unix domain socket '%s': ", sockpath);
	}
	else if (tcp_port <= 0) {	/* port -1 to disable */
		lprintf(CTDL_INFO, "Service has been manually disabled, skipping\n");
		free (message);
		free(newfcn);
		return;
	}
	else {
		newfcn->msock = ig_tcp_server(config.c_ip_addr,
					      tcp_port,
					      config.c_maxsessions, 
					      &error);
		snprintf(message, SIZ, "TCP port %s:%d: ", 
			 config.c_ip_addr, tcp_port);
	}

	if (newfcn->msock > 0) {
		ServiceHookTable = newfcn;
		strcat(message, "registered.");
		lprintf(CTDL_INFO, "%s\n", message);
	}
	else {
		AddPortError(message, error);
		strcat(message, "FAILED.");
		lprintf(CTDL_CRIT, "%s\n", message);
		free(error);
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

void CtdlDestroyServiceHook(void)
{
	struct ServiceFunctionHook *cur, *p;

	cur = ServiceHookTable;
	while (cur != NULL)
	{
		close(cur->msock);
		if (cur->sockpath) {
			lprintf(CTDL_INFO, "Closed UNIX domain socket %s\n",
				cur->sockpath);
		} else if (cur->tcp_port) {
			lprintf(CTDL_INFO, "Closed TCP port %d\n", cur->tcp_port);
		} else {
			lprintf(CTDL_INFO, "Unregistered unknown service\n");
		}
		p = cur->next;
		free(cur);
		cur = p;
	}
	ServiceHookTable = NULL;
}


void PerformSessionHooks(int EventType)
{
	struct SessionFunctionHook *fcn = NULL;

	for (fcn = SessionHookTable; fcn != NULL; fcn = fcn->next) {
		if (fcn->eventtype == EventType) {
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


int PerformRoomHooks(struct ctdlroom *target_room)
{
	struct RoomFunctionHook *fcn;
	int total_retval = 0;

	lprintf(CTDL_DEBUG, "Performing room hooks for <%s>\n", target_room->QRname);

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
