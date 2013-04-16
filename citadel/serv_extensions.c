/*
 * Citadel Dynamic Loading Module
 * Written by Brian Costello <btx@calyx.net>
 *
 * Copyright (c) 1987-2011 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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


int DebugModules = 0;
 
/*
 * Structure defentitions for hook tables
 */

typedef struct __LogDebugEntry {
	CtdlDbgFunction F;
	const char *Name;
	long Len;
	const int *LogP;
} LogDebugEntry;
HashList *LogDebugEntryTable = NULL;

typedef struct LogFunctionHook LogFunctionHook;
struct LogFunctionHook {
	LogFunctionHook *next;
	int loglevel;
	void (*h_function_pointer) (char *);
};
extern LogFunctionHook *LogHookTable;

typedef struct FixedOutputHook FixedOutputHook;
struct FixedOutputHook {
	FixedOutputHook *next;
	char content_type[64];
	void (*h_function_pointer) (char *, int);
};
extern FixedOutputHook *FixedOutputTable;



/*
 * SessionFunctionHook extensions are used for any type of hook for which
 * the context in which it's being called (which is determined by the event
 * type) will make it obvious for the hook function to know where to look for
 * pertinent data.
 */
typedef struct SessionFunctionHook SessionFunctionHook;
struct SessionFunctionHook {
	SessionFunctionHook *next;
	int Priority;
	void (*h_function_pointer) (void);
	int eventtype;
};
extern SessionFunctionHook *SessionHookTable;


/*
 * UserFunctionHook extensions are used for any type of hook which implements
 * an operation on a user or username (potentially) other than the one
 * operating the current session.
 */
typedef struct UserFunctionHook UserFunctionHook;
struct UserFunctionHook {
	UserFunctionHook *next;
	void (*h_function_pointer) (struct ctdluser *usbuf);
	int eventtype;
};
extern UserFunctionHook *UserHookTable;

/*
 * MessageFunctionHook extensions are used for hooks which implement handlers
 * for various types of message operations (save, read, etc.)
 */
typedef struct MessageFunctionHook MessageFunctionHook;
struct MessageFunctionHook {
	MessageFunctionHook *next;
	int (*h_function_pointer) (struct CtdlMessage *msg);
	int eventtype;
};
extern MessageFunctionHook *MessageHookTable;


/*
 * NetprocFunctionHook extensions are used for hooks which implement handlers
 * for incoming network messages.
 */
typedef struct NetprocFunctionHook NetprocFunctionHook;
struct NetprocFunctionHook {
	NetprocFunctionHook *next;
	int (*h_function_pointer) (struct CtdlMessage *msg, char *target_room);
};
extern NetprocFunctionHook *NetprocHookTable;


/*
 * DeleteFunctionHook extensions are used for hooks which get called when a
 * message is about to be deleted.
 */
typedef struct DeleteFunctionHook DeleteFunctionHook;
struct DeleteFunctionHook {
	DeleteFunctionHook *next;
	void (*h_function_pointer) (char *target_room, long msgnum);
};
extern DeleteFunctionHook *DeleteHookTable;


/*
 * ExpressMessageFunctionHook extensions are used for hooks which implement
 * the sending of an instant message through various channels.  Any function
 * registered should return the number of recipients to whom the message was
 * successfully transmitted.
 */
typedef struct XmsgFunctionHook XmsgFunctionHook;
struct XmsgFunctionHook {
	XmsgFunctionHook *next;
	int (*h_function_pointer) (char *, char *, char *, char *);
	int order;
};
extern XmsgFunctionHook *XmsgHookTable;




/*
 * RoomFunctionHook extensions are used for hooks which impliment room
 * processing functions when new messages are added EG. SIEVE.
 */
typedef struct RoomFunctionHook RoomFunctionHook;
struct RoomFunctionHook {
	RoomFunctionHook *next;
	int (*fcn_ptr) (struct ctdlroom *);
};
extern RoomFunctionHook *RoomHookTable;



typedef struct SearchFunctionHook SearchFunctionHook;
struct SearchFunctionHook {
	SearchFunctionHook *next;
	void (*fcn_ptr) (int *, long **, const char *);
	char *name;
};
extern SearchFunctionHook *SearchFunctionHookTable;


CleanupFunctionHook *CleanupHookTable = NULL;
CleanupFunctionHook *EVCleanupHookTable = NULL;
SessionFunctionHook *SessionHookTable = NULL;
UserFunctionHook *UserHookTable = NULL;
XmsgFunctionHook *XmsgHookTable = NULL;
MessageFunctionHook *MessageHookTable = NULL;
NetprocFunctionHook *NetprocHookTable = NULL;
DeleteFunctionHook *DeleteHookTable = NULL;
ServiceFunctionHook *ServiceHookTable = NULL;
FixedOutputHook *FixedOutputTable = NULL;
RoomFunctionHook *RoomHookTable = NULL;
SearchFunctionHook *SearchFunctionHookTable = NULL;

typedef struct ProtoFunctionHook ProtoFunctionHook;
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
	      "http://www.citadel.org/doku.php?id=faq:mastering_your_os:net#netstat")}
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

	MOD_syslog(LOG_EMERG, "%s", ChrPtr(Message));
	MOD_syslog(LOG_EMERG, "%s", ErrSubject);
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
	ProtoFunctionHook *p;

	if (GetHash(ProtoHookList, cmdbuf, 4, &vP) && (vP != NULL)) {
		p = (ProtoFunctionHook*) vP;
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

void CtdlRegisterDebugFlagHook(const char *Name, long Len, CtdlDbgFunction F, const int *LogP)
{
	LogDebugEntry *E;
	if (LogDebugEntryTable == NULL)
		LogDebugEntryTable = NewHash(1, NULL);
	E = (LogDebugEntry*) malloc(sizeof(LogDebugEntry));
	E->F = F;
	E->Name = Name;
	E->Len = Len;
	E->LogP = LogP;
	Put(LogDebugEntryTable, Name, Len, E, NULL);
	
}
void CtdlSetDebugLogFacilities(const char **Str, long n)
{
	StrBuf *Token = NULL;
	StrBuf *Buf = NULL;
	const char *ch;
	int i;
	int DoAll = 0;
	void *vptr;

	for (i=0; i < n; i++){
		if ((Str[i] != NULL) && !IsEmptyStr(Str[i])) {
			if (strcmp(Str[i], "all") == 0) {
				DoAll = 1;
				continue;
			}
			Buf = NewStrBufPlain(Str[i], -1);
			ch = NULL;
			if (Token == NULL)
				Token = NewStrBufPlain(NULL, StrLength(Buf));
			while ((ch != StrBufNOTNULL) &&
			       StrBufExtract_NextToken(Token, Buf, &ch, ',')) {
				if (GetHash(LogDebugEntryTable, SKEY(Token), &vptr) && 
				    (vptr != NULL))
				{
					LogDebugEntry *E = (LogDebugEntry*)vptr;
					E->F(1);
				}
			}
		}
		FreeStrBuf(&Buf);
	}
	FreeStrBuf(&Token);
	if (DoAll) {
		long HKLen;
		const char *ch;
		HashPos *Pos;

		Pos = GetNewHashPos(LogDebugEntryTable, 0);
		while (GetNextHashPos(LogDebugEntryTable, Pos, &HKLen, &ch, &vptr)) {
			LogDebugEntry *E = (LogDebugEntry*)vptr;
			E->F(1);
		}

		DeleteHashPos(&Pos);
	}
}
void cmd_log_get(char *argbuf)
{
	long HKLen;
	const char *ch;
	HashPos *Pos;
	void *vptr;

	if (CtdlAccessCheck(ac_aide)) return;

	cprintf("%d Log modules enabled:\n", LISTING_FOLLOWS);

	Pos = GetNewHashPos(LogDebugEntryTable, 0);

	while (GetNextHashPos(LogDebugEntryTable, Pos, &HKLen, &ch, &vptr)) {
		LogDebugEntry *E = (LogDebugEntry*)vptr;
		cprintf("%s|%d\n", ch, *E->LogP);
	}
	
	DeleteHashPos(&Pos);
	cprintf("000\n");
}
void cmd_log_set(char *argbuf)
{
	void *vptr;
	int lset;
	int wlen;
	char which[SIZ] = "";

	if (CtdlAccessCheck(ac_aide)) return;

	wlen = extract_token(which, argbuf, 0, '|', sizeof(which));
	if (wlen < 0) wlen = 0;
	lset = extract_int(argbuf, 1);
	if (lset != 0) lset = 1;
	if (GetHash(LogDebugEntryTable, which, wlen, &vptr) && 
	    (vptr != NULL))
	{
		LogDebugEntry *E = (LogDebugEntry*)vptr;
		E->F(lset);
		cprintf("%d %s|%d\n", CIT_OK, which, lset);
	}
	else {
		cprintf("%d Log setting %s not known\n", 
			ERROR, which);
	}
}
void CtdlDestroyDebugTable(void)
{

	DeleteHash(&LogDebugEntryTable);
}

void CtdlRegisterProtoHook(void (*handler) (char *), char *cmd, char *desc)
{
	ProtoFunctionHook *p;

	if (ProtoHookList == NULL)
		ProtoHookList = NewHash (1, FourHash);


	p = (ProtoFunctionHook *)
		malloc(sizeof(ProtoFunctionHook));

	if (p == NULL) {
		fprintf(stderr, "can't malloc new ProtoFunctionHook\n");
		exit(EXIT_FAILURE);
	}
	p->handler = handler;
	p->cmd = cmd;
	p->desc = desc;

	Put(ProtoHookList, cmd, 4, p, NULL);
	MOD_syslog(LOG_DEBUG, "Registered server command %s (%s)\n", cmd, desc);
}

void CtdlDestroyProtoHooks(void)
{

	DeleteHash(&ProtoHookList);
}


void CtdlRegisterCleanupHook(void (*fcn_ptr) (void))
{

	CleanupFunctionHook *newfcn;

	newfcn = (CleanupFunctionHook *)
	    malloc(sizeof(CleanupFunctionHook));
	newfcn->next = CleanupHookTable;
	newfcn->h_function_pointer = fcn_ptr;
	CleanupHookTable = newfcn;

	MODM_syslog(LOG_DEBUG, "Registered a new cleanup function\n");
}


void CtdlUnregisterCleanupHook(void (*fcn_ptr) (void))
{
	CleanupFunctionHook *cur, *p, *last;
	last = NULL;
	cur = CleanupHookTable;
	while (cur != NULL)
	{
		if (fcn_ptr == cur->h_function_pointer)
		{
			MODM_syslog(LOG_DEBUG, "Unregistered cleanup function\n");
			p = cur->next;

			free(cur);
			cur = NULL;

			if (last != NULL)
				last->next = p;
			else 
				CleanupHookTable = p;
			cur = p;
		}
		else {
			last = cur;
			cur = cur->next;
		}
	}
}


void CtdlDestroyCleanupHooks(void)
{
	CleanupFunctionHook *cur, *p;

	cur = CleanupHookTable;
	while (cur != NULL)
	{
		MODM_syslog(LOG_DEBUG, "Destroyed cleanup function\n");
		p = cur->next;
		free(cur);
		cur = p;
	}
	CleanupHookTable = NULL;
}

void CtdlRegisterEVCleanupHook(void (*fcn_ptr) (void))
{

	CleanupFunctionHook *newfcn;

	newfcn = (CleanupFunctionHook *)
	    malloc(sizeof(CleanupFunctionHook));
	newfcn->next = EVCleanupHookTable;
	newfcn->h_function_pointer = fcn_ptr;
	EVCleanupHookTable = newfcn;

	MODM_syslog(LOG_DEBUG, "Registered a new cleanup function\n");
}


void CtdlUnregisterEVCleanupHook(void (*fcn_ptr) (void))
{
	CleanupFunctionHook *cur, *p, *last;
	last = NULL;
	cur = EVCleanupHookTable;
	while (cur != NULL)
	{
		if (fcn_ptr == cur->h_function_pointer)
		{
			MODM_syslog(LOG_DEBUG, "Unregistered cleanup function\n");
			p = cur->next;

			free(cur);
			cur = NULL;

			if (last != NULL)
				last->next = p;
			else 
				EVCleanupHookTable = p;
			cur = p;
		}
		else {
			last = cur;
			cur = cur->next;
		}
	}
}


void CtdlDestroyEVCleanupHooks(void)
{
	CleanupFunctionHook *cur, *p;

	cur = EVCleanupHookTable;
	while (cur != NULL)
	{
		MODM_syslog(LOG_DEBUG, "Destroyed cleanup function\n");
		p = cur->next;
		cur->h_function_pointer();
		free(cur);
		cur = p;
	}
	EVCleanupHookTable = NULL;
}


void CtdlRegisterSessionHook(void (*fcn_ptr) (void), int EventType, int Priority)
{
	SessionFunctionHook *newfcn;

	newfcn = (SessionFunctionHook *)
	    malloc(sizeof(SessionFunctionHook));
	newfcn->Priority = Priority;
	newfcn->h_function_pointer = fcn_ptr;
	newfcn->eventtype = EventType;

	SessionFunctionHook **pfcn;
	pfcn = &SessionHookTable;
	while ((*pfcn != NULL) && 
	       ((*pfcn)->Priority < newfcn->Priority) &&
	       ((*pfcn)->next != NULL))
		pfcn = &(*pfcn)->next;
		
	newfcn->next = *pfcn;
	*pfcn = newfcn;
	
	MOD_syslog(LOG_DEBUG, "Registered a new session function (type %d Priority %d)\n",
		   EventType, Priority);
}


void CtdlUnregisterSessionHook(void (*fcn_ptr) (void), int EventType)
{
	SessionFunctionHook *cur, *p, *last;
	last = NULL;
	cur = SessionHookTable;
	while  (cur != NULL) {
		if ((fcn_ptr == cur->h_function_pointer) &&
		    (EventType == cur->eventtype))
		{
			MOD_syslog(LOG_DEBUG, "Unregistered session function (type %d)\n",
				   EventType);
			p = cur->next;

			free(cur);
			cur = NULL;

			if (last != NULL)
				last->next = p;
			else 
				SessionHookTable = p;
			cur = p;
		}
		else {
			last = cur;
			cur = cur->next;
		}
	}
}

void CtdlDestroySessionHooks(void)
{
	SessionFunctionHook *cur, *p;

	cur = SessionHookTable;
	while (cur != NULL)
	{
		MODM_syslog(LOG_DEBUG, "Destroyed session function\n");
		p = cur->next;
		free(cur);
		cur = p;
	}
	SessionHookTable = NULL;
}


void CtdlRegisterUserHook(void (*fcn_ptr) (ctdluser *), int EventType)
{

	UserFunctionHook *newfcn;

	newfcn = (UserFunctionHook *)
	    malloc(sizeof(UserFunctionHook));
	newfcn->next = UserHookTable;
	newfcn->h_function_pointer = fcn_ptr;
	newfcn->eventtype = EventType;
	UserHookTable = newfcn;

	MOD_syslog(LOG_DEBUG, "Registered a new user function (type %d)\n",
		   EventType);
}


void CtdlUnregisterUserHook(void (*fcn_ptr) (struct ctdluser *), int EventType)
{
	UserFunctionHook *cur, *p, *last;
	last = NULL;
	cur = UserHookTable;
	while (cur != NULL) {
		if ((fcn_ptr == cur->h_function_pointer) &&
		    (EventType == cur->eventtype))
		{
			MOD_syslog(LOG_DEBUG, "Unregistered user function (type %d)\n",
				   EventType);
			p = cur->next;

			free(cur);
			cur = NULL;

			if (last != NULL)
				last->next = p;
			else 
				UserHookTable = p;
			cur = p;
		}
		else {
			last = cur;
			cur = cur->next;
		}
	}
}

void CtdlDestroyUserHooks(void)
{
	UserFunctionHook *cur, *p;

	cur = UserHookTable;
	while (cur != NULL)
	{
		MODM_syslog(LOG_DEBUG, "Destroyed user function \n");
		p = cur->next;
		free(cur);
		cur = p;
	}
	UserHookTable = NULL;
}


void CtdlRegisterMessageHook(int (*handler)(struct CtdlMessage *),
				int EventType)
{

	MessageFunctionHook *newfcn;

	newfcn = (MessageFunctionHook *)
	    malloc(sizeof(MessageFunctionHook));
	newfcn->next = MessageHookTable;
	newfcn->h_function_pointer = handler;
	newfcn->eventtype = EventType;
	MessageHookTable = newfcn;

	MOD_syslog(LOG_DEBUG, "Registered a new message function (type %d)\n",
		   EventType);
}


void CtdlUnregisterMessageHook(int (*handler)(struct CtdlMessage *),
		int EventType)
{
	MessageFunctionHook *cur, *p, *last;
	last = NULL;
	cur = MessageHookTable;
	while (cur != NULL) {
		if ((handler == cur->h_function_pointer) &&
		    (EventType == cur->eventtype))
		{
			MOD_syslog(LOG_DEBUG, "Unregistered message function (type %d)\n",
				   EventType);
			p = cur->next;
			free(cur);
			cur = NULL;

			if (last != NULL)
				last->next = p;
			else 
				MessageHookTable = p;
			cur = p;
		}
		else {
			last = cur;
			cur = cur->next;
		}
	}
}

void CtdlDestroyMessageHook(void)
{
	MessageFunctionHook *cur, *p;

	cur = MessageHookTable; 
	while (cur != NULL)
	{
		MOD_syslog(LOG_DEBUG, "Destroyed message function (type %d)\n", cur->eventtype);
		p = cur->next;
		free(cur);
		cur = p;
	}
	MessageHookTable = NULL;
}


void CtdlRegisterRoomHook(int (*fcn_ptr)(struct ctdlroom *))
{
	RoomFunctionHook *newfcn;

	newfcn = (RoomFunctionHook *)
	    malloc(sizeof(RoomFunctionHook));
	newfcn->next = RoomHookTable;
	newfcn->fcn_ptr = fcn_ptr;
	RoomHookTable = newfcn;

	MODM_syslog(LOG_DEBUG, "Registered a new room function\n");
}


void CtdlUnregisterRoomHook(int (*fcn_ptr)(struct ctdlroom *))
{
	RoomFunctionHook *cur, *p, *last;
	last = NULL;
	cur = RoomHookTable;
	while (cur != NULL)
	{
		if (fcn_ptr == cur->fcn_ptr) {
			MODM_syslog(LOG_DEBUG, "Unregistered room function\n");
			p = cur->next;

			free(cur);
			cur = NULL;

			if (last != NULL)
				last->next = p;
			else 
				RoomHookTable = p;
			cur = p;
		}
		else {
			last = cur;
			cur = cur->next;
		}
	}
}


void CtdlDestroyRoomHooks(void)
{
	RoomFunctionHook *cur, *p;

	cur = RoomHookTable;
	while (cur != NULL)
	{
		MODM_syslog(LOG_DEBUG, "Destroyed room function\n");
		p = cur->next;
		free(cur);
		cur = p;
	}
	RoomHookTable = NULL;
}

void CtdlRegisterNetprocHook(int (*handler)(struct CtdlMessage *, char *) )
{
	NetprocFunctionHook *newfcn;

	newfcn = (NetprocFunctionHook *)
	    malloc(sizeof(NetprocFunctionHook));
	newfcn->next = NetprocHookTable;
	newfcn->h_function_pointer = handler;
	NetprocHookTable = newfcn;

	MODM_syslog(LOG_DEBUG, "Registered a new netproc function\n");
}


void CtdlUnregisterNetprocHook(int (*handler)(struct CtdlMessage *, char *) )
{
	NetprocFunctionHook *cur, *p, *last;

	cur = NetprocHookTable;
	last = NULL;

	while (cur != NULL) {
		if (handler == cur->h_function_pointer)
		{
			MODM_syslog(LOG_DEBUG, "Unregistered netproc function\n");
			p = cur->next;
			free(cur);
			if (last != NULL) {
				last->next = p;
			}
			else {
				NetprocHookTable = p;
			}
			cur = p;
		}
		else {
			last = cur;
			cur = cur->next;
		}
	}
}

void CtdlDestroyNetprocHooks(void)
{
	NetprocFunctionHook *cur, *p;

	cur = NetprocHookTable;
	while (cur != NULL)
	{
		MODM_syslog(LOG_DEBUG, "Destroyed netproc function\n");
		p = cur->next;
		free(cur);
		cur = p;
	}
	NetprocHookTable = NULL;
}


void CtdlRegisterDeleteHook(void (*handler)(char *, long) )
{
	DeleteFunctionHook *newfcn;

	newfcn = (DeleteFunctionHook *)
	    malloc(sizeof(DeleteFunctionHook));
	newfcn->next = DeleteHookTable;
	newfcn->h_function_pointer = handler;
	DeleteHookTable = newfcn;

	MODM_syslog(LOG_DEBUG, "Registered a new delete function\n");
}


void CtdlUnregisterDeleteHook(void (*handler)(char *, long) )
{
	DeleteFunctionHook *cur, *p, *last;

	last = NULL;
	cur = DeleteHookTable;
	while (cur != NULL) {
		if (handler == cur->h_function_pointer )
		{
			MODM_syslog(LOG_DEBUG, "Unregistered delete function\n");
			p = cur->next;
			free(cur);

			if (last != NULL)
				last->next = p;
			else
				DeleteHookTable = p;

			cur = p;
		}
		else {
			last = cur;
			cur = cur->next;
		}
	}
}
void CtdlDestroyDeleteHooks(void)
{
	DeleteFunctionHook *cur, *p;

	cur = DeleteHookTable;
	while (cur != NULL)
	{
		MODM_syslog(LOG_DEBUG, "Destroyed delete function\n");
		p = cur->next;
		free(cur);
		cur = p;		
	}
	DeleteHookTable = NULL;
}




void CtdlRegisterFixedOutputHook(char *content_type, void (*handler)(char *, int) )
{
	FixedOutputHook *newfcn;

	newfcn = (FixedOutputHook *)
	    malloc(sizeof(FixedOutputHook));
	newfcn->next = FixedOutputTable;
	newfcn->h_function_pointer = handler;
	safestrncpy(newfcn->content_type, content_type, sizeof newfcn->content_type);
	FixedOutputTable = newfcn;

	MOD_syslog(LOG_DEBUG, "Registered a new fixed output function for %s\n", newfcn->content_type);
}


void CtdlUnregisterFixedOutputHook(char *content_type)
{
	FixedOutputHook *cur, *p, *last;

	last = NULL;
	cur = FixedOutputTable;
	while (cur != NULL) {
		/* This will also remove duplicates if any */
		if (!strcasecmp(content_type, cur->content_type)) {
			MOD_syslog(LOG_DEBUG,
				   "Unregistered fixed output function for %s\n",
				   content_type);

			p = cur->next;
			free(cur);

			if (last != NULL)
				last->next = p;
			else
				FixedOutputTable = p;
			
			cur = p;
		}
		else
		{
			last = cur;
			cur = cur->next;
		}
	}
}

void CtdlDestroyFixedOutputHooks(void)
{
	FixedOutputHook *cur, *p;

	cur = FixedOutputTable; 
	while (cur != NULL)
	{
		MOD_syslog(LOG_DEBUG, "Destroyed fixed output function for %s\n", cur->content_type);
		p = cur->next;
		free(cur);
		cur = p;
		
	}
	FixedOutputTable = NULL;
}

/* returns nonzero if we found a hook and used it */
int PerformFixedOutputHooks(char *content_type, char *content, int content_length)
{
	FixedOutputHook *fcn;

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

	XmsgFunctionHook *newfcn;

	newfcn = (XmsgFunctionHook *) malloc(sizeof(XmsgFunctionHook));
	newfcn->next = XmsgHookTable;
	newfcn->order = order;
	newfcn->h_function_pointer = fcn_ptr;
	XmsgHookTable = newfcn;
	MOD_syslog(LOG_DEBUG, "Registered a new x-msg function (priority %d)\n", order);
}


void CtdlUnregisterXmsgHook(int (*fcn_ptr) (char *, char *, char *, char *), int order)
{
	XmsgFunctionHook *cur, *p, *last;

	last = NULL;
	cur = XmsgHookTable;
	while (cur != NULL) {
		/* This will also remove duplicates if any */
		if (fcn_ptr == cur->h_function_pointer &&
		    order == cur->order) {
			MOD_syslog(LOG_DEBUG, "Unregistered x-msg function "
				   "(priority %d)\n", order);
			p = cur->next;
			free(cur);

			if (last != NULL)
				last->next = p;
			else
				XmsgHookTable = p;
			
			cur = p;
		}
		else {
			last = cur;
			cur = cur->next;
		}
	}
}

void CtdlDestroyXmsgHooks(void)
{
	XmsgFunctionHook *cur, *p;

	cur = XmsgHookTable;
	while (cur != NULL)
	{
		MOD_syslog(LOG_DEBUG, "Destroyed x-msg function "
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
	ServiceFunctionHook *newfcn;
	char *message;
	char error[SIZ];

	strcpy(error, "");
	newfcn = (ServiceFunctionHook *) malloc(sizeof(ServiceFunctionHook));
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
		MOD_syslog(LOG_INFO, "Service %s has been manually disabled, skipping\n", ServiceName);
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
		MOD_syslog(LOG_INFO, "%s\n", message);
	}
	else {
		AddPortError(message, error);
		strcat(message, "FAILED.");
		MOD_syslog(LOG_CRIT, "%s\n", message);
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
	ServiceFunctionHook *cur, *p, *last;

	last = NULL;
	cur = ServiceHookTable;
	while (cur != NULL) {
		/* This will also remove duplicates if any */
		if (h_greeting_function == cur->h_greeting_function &&
		    h_command_function == cur->h_command_function &&
		    h_async_function == cur->h_async_function &&
		    tcp_port == cur->tcp_port && 
		    !(sockpath && cur->sockpath && strcmp(sockpath, cur->sockpath)) )
		{
			if (cur->msock > 0)
				close(cur->msock);
			if (sockpath) {
				MOD_syslog(LOG_INFO, "Closed UNIX domain socket %s\n",
					   sockpath);
				unlink(sockpath);
			} else if (tcp_port) {
				MOD_syslog(LOG_INFO, "Closed TCP port %d\n", tcp_port);
			} else {
				MOD_syslog(LOG_INFO, "Unregistered service \"%s\"\n", cur->ServiceName);
			}
			p = cur->next;
			free(cur);
			if (last != NULL)
				last->next = p;
			else
				ServiceHookTable = p;
			cur = p;
		}
		else {
			last = cur;
			cur = cur->next;
		}
	}
}


void CtdlShutdownServiceHooks(void)
{
	/* sort of a duplicate of close_masters() but called earlier */
	ServiceFunctionHook *cur;

	cur = ServiceHookTable;
	while (cur != NULL) 
	{
		if (cur->msock != -1)
		{
			close(cur->msock);
			cur->msock = -1;
			if (cur->sockpath != NULL){
				MOD_syslog(LOG_INFO, "[%s] Closed UNIX domain socket %s\n",
					   cur->ServiceName,
					   cur->sockpath);
				unlink(cur->sockpath);
			} else {
				MOD_syslog(LOG_INFO, "[%s] closing service\n", 
					   cur->ServiceName);
			}
		}
		cur = cur->next;
	}
}

void CtdlDestroyServiceHook(void)
{
	const char *Text;
	ServiceFunctionHook *cur, *p;

	cur = ServiceHookTable;
	while (cur != NULL)
	{
		if (cur->msock != -1)
		{
			close(cur->msock);
			Text = "Closed";
		}
		else
		{
			Text = " Not closing again";
		}

		if (cur->sockpath) {
			MOD_syslog(LOG_INFO, "%s UNIX domain socket %s\n",
				   Text,
				   cur->sockpath);
			unlink(cur->sockpath);
		} else if (cur->tcp_port) {
			MOD_syslog(LOG_INFO, "%s TCP port %d\n", Text, cur->tcp_port);
		} else {
			MOD_syslog(LOG_INFO, "Destroyed service \"%s\"\n", cur->ServiceName);
		}
		p = cur->next;
		free(cur);
		cur = p;
	}
	ServiceHookTable = NULL;
}

void CtdlRegisterSearchFuncHook(void (*fcn_ptr)(int *, long **, const char *), char *name)
{
	SearchFunctionHook *newfcn;

	if (!name || !fcn_ptr) {
		return;
	}
	
	newfcn = (SearchFunctionHook *)
	    malloc(sizeof(SearchFunctionHook));
	newfcn->next = SearchFunctionHookTable;
	newfcn->name = name;
	newfcn->fcn_ptr = fcn_ptr;
	SearchFunctionHookTable = newfcn;

	MOD_syslog(LOG_DEBUG, "Registered a new search function (%s)\n", name);
}

void CtdlUnregisterSearchFuncHook(void (*fcn_ptr)(int *, long **, const char *), char *name)
{
	SearchFunctionHook *cur, *p, *last;
	
	last = NULL;
	cur = SearchFunctionHookTable;
	while (cur != NULL) {
		if (fcn_ptr &&
		    (cur->fcn_ptr == fcn_ptr) &&
		    name && !strcmp(name, cur->name))
		{
			MOD_syslog(LOG_DEBUG, "Unregistered search function(%s)\n", name);
			p = cur->next;
			free (cur);
			if (last != NULL)
				last->next = p;
			else
				SearchFunctionHookTable = p;
			cur = p;
		}
		else {
			last = cur;
			cur = cur->next;
		}
	}
}

void CtdlDestroySearchHooks(void)
{
        SearchFunctionHook *cur, *p;

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
	SearchFunctionHook *fcn = NULL;

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
	SessionFunctionHook *fcn = NULL;

	for (fcn = SessionHookTable; fcn != NULL; fcn = fcn->next) {
		if (fcn->eventtype == EventType) {
			if (EventType == EVT_TIMER) {
				pthread_setspecific(MyConKey, NULL);	/* for every hook */
			}
			(*fcn->h_function_pointer) ();
		}
	}
}

void PerformUserHooks(ctdluser *usbuf, int EventType)
{
	UserFunctionHook *fcn = NULL;

	for (fcn = UserHookTable; fcn != NULL; fcn = fcn->next) {
		if (fcn->eventtype == EventType) {
			(*fcn->h_function_pointer) (usbuf);
		}
	}
}

int PerformMessageHooks(struct CtdlMessage *msg, int EventType)
{
	MessageFunctionHook *fcn = NULL;
	int total_retval = 0;

	/* Other code may elect to protect this message from server-side
	 * handlers; if this is the case, don't do anything.
	MOD_syslog(LOG_DEBUG, "** Event type is %d, flags are %d\n", EventType, msg->cm_flags);
	 */
	if (msg->cm_flags & CM_SKIP_HOOKS) {
		MODM_syslog(LOG_DEBUG, "Skipping hooks\n");
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
	RoomFunctionHook *fcn;
	int total_retval = 0;

	MOD_syslog(LOG_DEBUG, "Performing room hooks for <%s>\n", target_room->QRname);

	for (fcn = RoomHookTable; fcn != NULL; fcn = fcn->next) {
		total_retval = total_retval + (*fcn->fcn_ptr) (target_room);
	}

	/* Return the sum of the return codes from the hook functions.
	 */
	return total_retval;
}


int PerformNetprocHooks(struct CtdlMessage *msg, char *target_room)
{
	NetprocFunctionHook *fcn;
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
	DeleteFunctionHook *fcn;

	for (fcn = DeleteHookTable; fcn != NULL; fcn = fcn->next) {
		(*fcn->h_function_pointer) (room, msgnum);
	}
}





int PerformXmsgHooks(char *sender, char *sender_email, char *recp, char *msg)
{
	XmsgFunctionHook *fcn;
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

void DebugModulesEnable(const int n)
{
	DebugModules = n;
}
CTDL_MODULE_INIT(modules)
{
	if (!threading) {
		CtdlRegisterDebugFlagHook(HKEY("modules"), DebugModulesEnable, &DebugModules);

		CtdlRegisterProtoHook(cmd_log_get, "LOGP", "Print Log-parameters");
		CtdlRegisterProtoHook(cmd_log_set, "LOGS", "Set Log-parameters");
	}
	return "modules";
}
