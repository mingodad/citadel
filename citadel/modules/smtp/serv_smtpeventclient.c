/*
 * This module is an SMTP and ESMTP implementation for the Citadel system.
 * It is compliant with all of the following:
 *
 * RFC  821 - Simple Mail Transfer Protocol
 * RFC  876 - Survey of SMTP Implementations
 * RFC 1047 - Duplicate messages and SMTP
 * RFC 1652 - 8 bit MIME
 * RFC 1869 - Extended Simple Mail Transfer Protocol
 * RFC 1870 - SMTP Service Extension for Message Size Declaration
 * RFC 2033 - Local Mail Transfer Protocol
 * RFC 2197 - SMTP Service Extension for Command Pipelining
 * RFC 2476 - Message Submission
 * RFC 2487 - SMTP Service Extension for Secure SMTP over TLS
 * RFC 2554 - SMTP Service Extension for Authentication
 * RFC 2821 - Simple Mail Transfer Protocol
 * RFC 2822 - Internet Message Format
 * RFC 2920 - SMTP Service Extension for Command Pipelining
 *  
 * The VRFY and EXPN commands have been removed from this implementation
 * because nobody uses these commands anymore, except for spammers.
 *
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
#include "config.h"
#include "control.h"
#include "user_ops.h"
#include "database.h"
#include "msgbase.h"
#include "internet_addressing.h"
#include "genstamp.h"
#include "domain.h"
#include "clientsocket.h"
#include "locate_host.h"
#include "citadel_dirs.h"

#include "ctdl_module.h"

#include "smtp_util.h"
#include "event_client.h"

#ifdef EXPERIMENTAL_SMTP_EVENT_CLIENT
HashList *QItemHandlers = NULL;

citthread_mutex_t ActiveQItemsLock;
HashList *ActiveQItems = NULL;

int run_queue_now = 0;	/* Set to 1 to ignore SMTP send retry times */
int MsgCount = 0;
/*****************************************************************************/
/*               SMTP CLIENT (Queue Management) STUFF                        */
/*****************************************************************************/

#define MaxAttempts 15
typedef struct _delivery_attempt {
	time_t when;
	time_t retry;
}DeliveryAttempt;

typedef struct _mailq_entry {
	DeliveryAttempt Attempts[MaxAttempts];
	int nAttempts;
	StrBuf *Recipient;
	StrBuf *StatusMessage;
	int Status;
	int n;
	int Active;
}MailQEntry;
void FreeMailQEntry(void *qv)
{
	MailQEntry *Q = qv;
	FreeStrBuf(&Q->Recipient);
	FreeStrBuf(&Q->StatusMessage);
	free(Q);
}

typedef struct queueitem {
	long MessageID;
	long QueMsgID;
	int FailNow;
	HashList *MailQEntries;
	MailQEntry *Current; /* copy of the currently parsed item in the MailQEntries list; if null add a new one. */
	DeliveryAttempt LastAttempt;
	long ActiveDeliveries;
	StrBuf *EnvelopeFrom;
	StrBuf *BounceTo;
} OneQueItem;
typedef void (*QItemHandler)(OneQueItem *Item, StrBuf *Line, const char **Pos);

/*****************************************************************************/
/*               SMTP CLIENT (OUTBOUND PROCESSING) STUFF                     */
/*****************************************************************************/

typedef enum _eSMTP_C_States {
	eConnect, 
	eEHLO,
	eHELO,
	eSMTPAuth,
	eFROM,
	eRCPT,
	eDATA,
	eDATABody,
	eDATATerminateBody,
	eQUIT,
	eMaxSMTPC
} eSMTP_C_States;

const long SMTP_C_ReadTimeouts[eMaxSMTPC] = {
	90, /* Greeting... */
	30, /* EHLO */
	30, /* HELO */
	30, /* Auth */
	30, /* From */
	30, /* RCPT */
	30, /* DATA */
	90, /* DATABody */
	900, /* end of body... */
	30  /* QUIT */
};
/*
const long SMTP_C_SendTimeouts[eMaxSMTPC] = {

}; */
const char *ReadErrors[eMaxSMTPC] = {
	"Connection broken during SMTP conversation",
	"Connection broken during SMTP EHLO",
	"Connection broken during SMTP HELO",
	"Connection broken during SMTP AUTH",
	"Connection broken during SMTP MAIL FROM",
	"Connection broken during SMTP RCPT",
	"Connection broken during SMTP DATA",
	"Connection broken during SMTP message transmit",
	""/* quit reply, don't care. */
};


typedef struct _stmp_out_msg {
	MailQEntry *MyQEntry;
	OneQueItem *MyQItem;
	long n;
	AsyncIO IO;

	eSMTP_C_States State;

	struct ares_mx_reply *AllMX;
	struct ares_mx_reply *CurrMX;
	const char *mx_port;
	const char *mx_host;

	struct hostent *OneMX;


	char mx_user[1024];
	char mx_pass[1024];
	StrBuf *msgtext;
	char *envelope_from;
	char user[1024];
	char node[1024];
	char name[1024];
	char mailfrom[1024];
} SmtpOutMsg;
void DeleteSmtpOutMsg(void *v)
{
	SmtpOutMsg *Msg = v;
	FreeStrBuf(&Msg->msgtext);
	FreeAsyncIOContents(&Msg->IO);
	free(Msg);
}

eNextState SMTP_C_Timeout(void *Data);
eNextState SMTP_C_ConnFail(void *Data);
eNextState SMTP_C_DispatchReadDone(void *Data);
eNextState SMTP_C_DispatchWriteDone(void *Data);
eNextState SMTP_C_Terminate(void *Data);
eNextState SMTP_C_MXLookup(void *Data);

typedef eNextState (*SMTPReadHandler)(SmtpOutMsg *Msg);
typedef eNextState (*SMTPSendHandler)(SmtpOutMsg *Msg);




void FreeQueItem(OneQueItem **Item)
{
	DeleteHash(&(*Item)->MailQEntries);
	FreeStrBuf(&(*Item)->EnvelopeFrom);
	FreeStrBuf(&(*Item)->BounceTo);
	free(*Item);
	Item = NULL;
}
void HFreeQueItem(void *Item)
{
	FreeQueItem((OneQueItem**)&Item);
}


/* inspect recipients with a status of: 
 * - 0 (no delivery yet attempted) 
 * - 3/4 (transient errors
 *        were experienced and it's time to try again)
 */
int CountActiveQueueEntries(OneQueItem *MyQItem)
{
	HashPos  *It;
	long len;
	const char *Key;
	void *vQE;

	MyQItem->ActiveDeliveries = 0;
	It = GetNewHashPos(MyQItem->MailQEntries, 0);
	while (GetNextHashPos(MyQItem->MailQEntries, It, &len, &Key, &vQE))
	{
		MailQEntry *ThisItem = vQE;
		if ((ThisItem->Status == 0) || 
		    (ThisItem->Status == 3) ||
		    (ThisItem->Status == 4))
		{
			MyQItem->ActiveDeliveries++;
			ThisItem->Active = 1;
		}
		else 
			ThisItem->Active = 0;
	}
	DeleteHashPos(&It);
	return MyQItem->ActiveDeliveries;
}

OneQueItem *DeserializeQueueItem(StrBuf *RawQItem, long QueMsgID)
{
	OneQueItem *Item;
	const char *pLine = NULL;
	StrBuf *Line;
	StrBuf *Token;
	void *v;

	Item = (OneQueItem*)malloc(sizeof(OneQueItem));
	memset(Item, 0, sizeof(OneQueItem));
	Item->LastAttempt.retry = SMTP_RETRY_INTERVAL;
	Item->MessageID = -1;
	Item->QueMsgID = QueMsgID;

	citthread_mutex_lock(&ActiveQItemsLock);
	if (GetHash(ActiveQItems, 
		    IKEY(Item->QueMsgID), 
		    &v))
	{
		/* WHOOPS. somebody else is already working on this. */
		citthread_mutex_unlock(&ActiveQItemsLock);
		FreeQueItem(&Item);
		return NULL;
	}
	else {
		/* mark our claim on this. */
		Put(ActiveQItems, 
		    IKEY(Item->QueMsgID),
		    Item,
		    HFreeQueItem);
		citthread_mutex_unlock(&ActiveQItemsLock);
	}

	Token = NewStrBuf();
	Line = NewStrBufPlain(NULL, 128);
	while (pLine != StrBufNOTNULL) {
		const char *pItemPart = NULL;
		void *vHandler;

		StrBufExtract_NextToken(Line, RawQItem, &pLine, '\n');
		if (StrLength(Line) == 0) continue;
		StrBufExtract_NextToken(Token, Line, &pItemPart, '|');
		if (GetHash(QItemHandlers, SKEY(Token), &vHandler))
		{
			QItemHandler H;
			H = (QItemHandler) vHandler;
			H(Item, Line, &pItemPart);
		}
	}
	FreeStrBuf(&Line);
	FreeStrBuf(&Token);
	return Item;
}

StrBuf *SerializeQueueItem(OneQueItem *MyQItem)
{
	StrBuf *QMessage;
	HashPos  *It;
	const char *Key;
	long len;
	void *vQE;

	QMessage = NewStrBufPlain(NULL, SIZ);
	StrBufPrintf(QMessage, "Content-type: %s\n", SPOOLMIME);

//		   "attempted|%ld\n"  "retry|%ld\n",, (long)time(NULL), (long)retry );
	StrBufAppendBufPlain(QMessage, HKEY("\nmsgid|"), 0);
	StrBufAppendPrintf(QMessage, "%ld", MyQItem->MessageID);

	if (StrLength(MyQItem->BounceTo) > 0) {
		StrBufAppendBufPlain(QMessage, HKEY("\nbounceto|"), 0);
		StrBufAppendBuf(QMessage, MyQItem->BounceTo, 0);
	}

	if (StrLength(MyQItem->EnvelopeFrom) > 0) {
		StrBufAppendBufPlain(QMessage, HKEY("\nenvelope_from|"), 0);
		StrBufAppendBuf(QMessage, MyQItem->EnvelopeFrom, 0);
	}

	It = GetNewHashPos(MyQItem->MailQEntries, 0);
	while (GetNextHashPos(MyQItem->MailQEntries, It, &len, &Key, &vQE))
	{
		MailQEntry *ThisItem = vQE;
		int i;

		if (!ThisItem->Active)
			continue; /* skip already sent ones from the spoolfile. */

		for (i=0; i < ThisItem->nAttempts; i++) {
			StrBufAppendBufPlain(QMessage, HKEY("\nretry|"), 0);
			StrBufAppendPrintf(QMessage, "%ld", 
					   ThisItem->Attempts[i].retry);

			StrBufAppendBufPlain(QMessage, HKEY("\nattempted|"), 0);
			StrBufAppendPrintf(QMessage, "%ld", 
					   ThisItem->Attempts[i].when);
		}
		StrBufAppendBufPlain(QMessage, HKEY("\nremote|"), 0);
		StrBufAppendBuf(QMessage, ThisItem->Recipient, 0);
		StrBufAppendBufPlain(QMessage, HKEY("|"), 0);
		StrBufAppendPrintf(QMessage, "%d", ThisItem->Status);
		StrBufAppendBufPlain(QMessage, HKEY("|"), 0);
		StrBufAppendBuf(QMessage, ThisItem->StatusMessage, 0);
	}
	DeleteHashPos(&It);
	StrBufAppendBufPlain(QMessage, HKEY("\n"), 0);	
	return QMessage;
}

void FinalizeMessageSend(SmtpOutMsg *Msg)
{
	int IDestructQueItem;
	HashPos  *It;

	citthread_mutex_lock(&ActiveQItemsLock);
	Msg->MyQItem->ActiveDeliveries--;
	IDestructQueItem = Msg->MyQItem->ActiveDeliveries == 0;
	citthread_mutex_unlock(&ActiveQItemsLock);

	if (IDestructQueItem) {
		int nRemain;
		StrBuf *MsgData;

		nRemain = CountActiveQueueEntries(Msg->MyQItem);

		if (nRemain > 0) 
			MsgData = SerializeQueueItem(Msg->MyQItem);
		/*
		 * Uncompleted delivery instructions remain, so delete the old
		 * instructions and replace with the updated ones.
		 */
		CtdlDeleteMessages(SMTP_SPOOLOUT_ROOM, &Msg->MyQItem->QueMsgID, 1, "");

	/* Generate 'bounce' messages * /
	   smtp_do_bounce(instr); */
		if (nRemain > 0) {
			struct CtdlMessage *msg;
			msg = malloc(sizeof(struct CtdlMessage));
			memset(msg, 0, sizeof(struct CtdlMessage));
			msg->cm_magic = CTDLMESSAGE_MAGIC;
			msg->cm_anon_type = MES_NORMAL;
			msg->cm_format_type = FMT_RFC822;
			msg->cm_fields['M'] = SmashStrBuf(&MsgData);

			CtdlSubmitMsg(msg, NULL, SMTP_SPOOLOUT_ROOM, QP_EADDR);
			CtdlFreeMessage(msg);
		}
		It = GetNewHashPos(Msg->MyQItem->MailQEntries, 0);
		citthread_mutex_lock(&ActiveQItemsLock);
		{
			GetHashPosFromKey(ActiveQItems, IKEY(Msg->MyQItem->MessageID), It);
			DeleteEntryFromHash(ActiveQItems, It);
		}
		citthread_mutex_unlock(&ActiveQItemsLock);
		DeleteHashPos(&It);
	}
	
/// TODO : else free message...
	close(Msg->IO.sock);
	DeleteSmtpOutMsg(Msg);
}

eReadState SMTP_C_ReadServerStatus(AsyncIO *IO)
{
	eReadState Finished = eBufferNotEmpty; 

	while (Finished == eBufferNotEmpty) {
		Finished = StrBufChunkSipLine(IO->IOBuf, &IO->RecvBuf);
		
		switch (Finished) {
		case eMustReadMore: /// read new from socket... 
			return Finished;
			break;
		case eBufferNotEmpty: /* shouldn't happen... */
		case eReadSuccess: /// done for now...
			if (StrLength(IO->IOBuf) < 4)
				continue;
			if (ChrPtr(IO->IOBuf)[3] == '-')
				Finished = eBufferNotEmpty;
			else 
				return Finished;
			break;
		case eReadFail: /// WHUT?
			///todo: shut down! 
			break;
		}
	}
	return Finished;
}

/**
 * this one has to have the context for loading the message via the redirect buffer...
 */
StrBuf *smtp_load_msg(OneQueItem *MyQItem)
{
	CitContext *CCC=CC;
	StrBuf *SendMsg;
	
	CCC->redirect_buffer = NewStrBufPlain(NULL, SIZ);
	CtdlOutputMsg(MyQItem->MessageID, MT_RFC822, HEADERS_ALL, 0, 1, NULL, (ESC_DOT|SUPPRESS_ENV_TO) );
	SendMsg = CCC->redirect_buffer;
	CCC->redirect_buffer = NULL;
	if ((StrLength(SendMsg) > 0) && 
	    ChrPtr(SendMsg)[StrLength(SendMsg) - 1] != '\n') {
		CtdlLogPrintf(CTDL_WARNING, 
			      "SMTP client[%ld]: Possible problem: message did not "
			      "correctly terminate. (expecting 0x10, got 0x%02x)\n",
			      MsgCount, //yes uncool, but best choice here... 
			      ChrPtr(SendMsg)[StrLength(SendMsg) - 1] );
		StrBufAppendBufPlain(SendMsg, HKEY("\r\n"), 0);
	}
	return SendMsg;
}


int smtp_resolve_recipients(SmtpOutMsg *SendMsg)
{
	const char *ptr;
	char buf[1024];
	int scan_done;
	int lp, rp;
	int i;

	/* Parse out the host portion of the recipient address */
	process_rfc822_addr(ChrPtr(SendMsg->MyQEntry->Recipient), 
			    SendMsg->user, 
			    SendMsg->node, 
			    SendMsg->name);

	CtdlLogPrintf(CTDL_DEBUG, "SMTP client[%ld]: Attempting delivery to <%s> @ <%s> (%s)\n",
		      SendMsg->n, SendMsg->user, SendMsg->node, SendMsg->name);
	/* If no envelope_from is supplied, extract one from the message */
	if ( (SendMsg->envelope_from == NULL) || 
	     (IsEmptyStr(SendMsg->envelope_from)) ) {
		SendMsg->mailfrom[0] = '\0';
		scan_done = 0;
		ptr = ChrPtr(SendMsg->msgtext);
		do {
			if (ptr = cmemreadline(ptr, buf, sizeof buf), *ptr == 0) {
				scan_done = 1;
			}
			if (!strncasecmp(buf, "From:", 5)) {
				safestrncpy(SendMsg->mailfrom, &buf[5], sizeof SendMsg->mailfrom);
				striplt(SendMsg->mailfrom);
				for (i=0; SendMsg->mailfrom[i]; ++i) {
					if (!isprint(SendMsg->mailfrom[i])) {
						strcpy(&SendMsg->mailfrom[i], &SendMsg->mailfrom[i+1]);
						i=0;
					}
				}
	
				/* Strip out parenthesized names */
				lp = (-1);
				rp = (-1);
				for (i=0; !IsEmptyStr(SendMsg->mailfrom + i); ++i) {
					if (SendMsg->mailfrom[i] == '(') lp = i;
					if (SendMsg->mailfrom[i] == ')') rp = i;
				}
				if ((lp>0)&&(rp>lp)) {
					strcpy(&SendMsg->mailfrom[lp-1], &SendMsg->mailfrom[rp+1]);
				}
	
				/* Prefer brokketized names */
				lp = (-1);
				rp = (-1);
				for (i=0; !IsEmptyStr(SendMsg->mailfrom + i); ++i) {
					if (SendMsg->mailfrom[i] == '<') lp = i;
					if (SendMsg->mailfrom[i] == '>') rp = i;
				}
				if ( (lp>=0) && (rp>lp) ) {
					SendMsg->mailfrom[rp] = 0;
					memmove(SendMsg->mailfrom, 
						&SendMsg->mailfrom[lp + 1], 
						rp - lp);
				}
	
				scan_done = 1;
			}
		} while (scan_done == 0);
		if (IsEmptyStr(SendMsg->mailfrom)) strcpy(SendMsg->mailfrom, "someone@somewhere.org");
		stripallbut(SendMsg->mailfrom, '<', '>');
		SendMsg->envelope_from = SendMsg->mailfrom;
	}

	return 0;
}


#define SMTP_ERROR(WHICH_ERR, ERRSTR) {SendMsg->MyQEntry->Status = WHICH_ERR; StrBufAppendBufPlain(SendMsg->MyQEntry->StatusMessage, HKEY(ERRSTR), 0); return eAbort; }
#define SMTP_VERROR(WHICH_ERR) { SendMsg->MyQEntry->Status = WHICH_ERR; StrBufAppendBufPlain(SendMsg->MyQEntry->StatusMessage, &ChrPtr(SendMsg->IO.IOBuf)[4], -1, 0); return eAbort; }
#define SMTP_IS_STATE(WHICH_STATE) (ChrPtr(SendMsg->IO.IOBuf)[0] == WHICH_STATE)

#define SMTP_DBG_SEND() CtdlLogPrintf(CTDL_DEBUG, "SMTP client[%ld]: > %s\n", SendMsg->n, ChrPtr(SendMsg->IO.IOBuf))
#define SMTP_DBG_READ() CtdlLogPrintf(CTDL_DEBUG, "SMTP client[%ld]: < %s\n", SendMsg->n, ChrPtr(SendMsg->IO.IOBuf))

/*
void connect_one_smtpsrv_xamine_result(void *Ctx, 
				       int status,
				       int timeouts,
				       struct hostent *hostent)
{
	SmtpOutMsg *SendMsg = Ctx;

	CtdlLogPrintf(CTDL_DEBUG, "SMTP client[%ld]: connecting [%s:%s]!\n", 
		      SendMsg->n, SendMsg->mx_host, SendMsg->mx_port);

	SendMsg->IO.SendBuf.Buf = NewStrBufPlain(NULL, 1024);
	SendMsg->IO.RecvBuf.Buf = NewStrBufPlain(NULL, 1024);
	SendMsg->IO.IOBuf = NewStrBuf();
	SendMsg->IO.ErrMsg = SendMsg->MyQEntry->StatusMessage;


	SendMsg->IO.SendBuf.fd = 
	SendMsg->IO.RecvBuf.fd = 
	SendMsg->IO.sock = sock_connect(SendMsg->mx_host, SendMsg->mx_port);

	StrBufPrintf(SendMsg->MyQEntry->StatusMessage, 
		     "Could not connect: %s", strerror(errno));


	if (SendMsg->IO.sock < 0) {
		if (errno > 0) {
			StrBufPlain(SendMsg->MyQEntry->StatusMessage, 
				    strerror(errno), -1);
		}
		else {
			StrBufPrintf(SendMsg->MyQEntry->StatusMessage, 
				     "Unable to connect to %s : %s\n", 
				     SendMsg->mx_host, SendMsg->mx_port);
		}
	}
	/// hier: naechsten mx ausprobieren.
	if (SendMsg->IO.sock < 0) {
		SendMsg->MyQEntry->Status = 4;	/* dsn is already filled in * /
		//// hier: abbrechen & bounce.
		return;
	}
/*

	InitEventIO(&SendMsg->IO, SendMsg, 
		    SMTP_C_DispatchReadDone, 
		    SMTP_C_DispatchWriteDone, 
		    SMTP_C_Terminate,
		    SMTP_C_Timeout,
		    SMTP_C_ConnFail,
		    SMTP_C_MXLookup,
		    SMTP_C_ReadServerStatus,
		    1);
* /
	return;
}
*/

void get_one_mx_host_name_done(void *Ctx, 
			       int status,
			       int timeouts,
			       struct hostent *hostent)
{
	SmtpOutMsg *SendMsg = Ctx;
	if ((status == ARES_SUCCESS) && (hostent != NULL) ) {

			SendMsg->IO.HEnt = hostent;
			InitEventIO(&SendMsg->IO, SendMsg, 
				    SMTP_C_DispatchReadDone, 
				    SMTP_C_DispatchWriteDone, 
				    SMTP_C_Terminate,
				    SMTP_C_Timeout,
				    SMTP_C_ConnFail,
				    SMTP_C_ReadServerStatus,
				    1);

	}
}

const char *DefaultMXPort = "25";
void connect_one_smtpsrv(SmtpOutMsg *SendMsg)
{
	//char *endpart;
	//char buf[SIZ];

	SendMsg->mx_port = DefaultMXPort;


/* TODO: Relay!
	*SendMsg->mx_user =  '\0';
	*SendMsg->mx_pass = '\0';
	if (num_tokens(buf, '@') > 1) {
		strcpy (SendMsg->mx_user, buf);
		endpart = strrchr(SendMsg->mx_user, '@');
		*endpart = '\0';
		strcpy (SendMsg->mx_host, endpart + 1);
		endpart = strrchr(SendMsg->mx_user, ':');
		if (endpart != NULL) {
			strcpy(SendMsg->mx_pass, endpart+1);
			*endpart = '\0';
		}

	endpart = strrchr(SendMsg->mx_host, ':');
	if (endpart != 0){
		*endpart = '\0';
		strcpy(SendMsg->mx_port, endpart + 1);
	}		
	}
	else
*/
	SendMsg->mx_host = SendMsg->CurrMX->host;
	SendMsg->CurrMX = SendMsg->CurrMX->next;

	CtdlLogPrintf(CTDL_DEBUG, 
		      "SMTP client[%ld]: connecting to %s : %s ...\n", 
		      SendMsg->n, 
		      SendMsg->mx_host, 
		      SendMsg->mx_port);

	ares_gethostbyname(SendMsg->IO.DNSChannel,
			   SendMsg->mx_host,   
			   AF_INET6, /* it falls back to ipv4 in doubt... */
			   get_one_mx_host_name_done,
			   &SendMsg->IO);
/*
	if (!QueueQuery(ns_t_a, 
			SendMsg->mx_host, 
			&SendMsg->IO, 
			connect_one_smtpsrv_xamine_result))
	{
		/// TODO: abort
	}
*/
}


eNextState SMTPC_read_greeting(SmtpOutMsg *SendMsg)
{
	/* Process the SMTP greeting from the server */
	SMTP_DBG_READ();

	if (!SMTP_IS_STATE('2')) {
		if (SMTP_IS_STATE('4')) 
			SMTP_VERROR(4)
		else 
			SMTP_VERROR(5)
	}
	return eSendReply;
}

eNextState SMTPC_send_EHLO(SmtpOutMsg *SendMsg)
{
	/* At this point we know we are talking to a real SMTP server */

	/* Do a EHLO command.  If it fails, try the HELO command. */
	StrBufPrintf(SendMsg->IO.SendBuf.Buf,
		     "EHLO %s\r\n", config.c_fqdn);

	SMTP_DBG_SEND();
	return eReadMessage;
}

eNextState SMTPC_read_EHLO_reply(SmtpOutMsg *SendMsg)
{
	SMTP_DBG_READ();

	if (SMTP_IS_STATE('2')) {
		SendMsg->State ++;
		if (IsEmptyStr(SendMsg->mx_user))
			SendMsg->State ++; /* Skip auth... */
	}
	/* else we fall back to 'helo' */
	return eSendReply;
}

eNextState STMPC_send_HELO(SmtpOutMsg *SendMsg)
{
	StrBufPrintf(SendMsg->IO.SendBuf.Buf,
		     "HELO %s\r\n", config.c_fqdn);

	SMTP_DBG_SEND();
	return eReadMessage;
}

eNextState SMTPC_read_HELO_reply(SmtpOutMsg *SendMsg)
{
	SMTP_DBG_READ();

	if (!SMTP_IS_STATE('2')) {
		if (SMTP_IS_STATE('4'))
			SMTP_VERROR(4)
		else 
			SMTP_VERROR(5)
	}
	if (!IsEmptyStr(SendMsg->mx_user))
		SendMsg->State ++; /* Skip auth... */
	return eSendReply;
}

eNextState SMTPC_send_auth(SmtpOutMsg *SendMsg)
{
	char buf[SIZ];
	char encoded[1024];

	/* Do an AUTH command if necessary */
	sprintf(buf, "%s%c%s%c%s", 
		SendMsg->mx_user, '\0', 
		SendMsg->mx_user, '\0', 
		SendMsg->mx_pass);
	CtdlEncodeBase64(encoded, buf, 
			 strlen(SendMsg->mx_user) + 
			 strlen(SendMsg->mx_user) + 
			 strlen(SendMsg->mx_pass) + 2, 0);
	StrBufPrintf(SendMsg->IO.SendBuf.Buf,
		     "AUTH PLAIN %s\r\n", encoded);
	
	SMTP_DBG_SEND();
	return eReadMessage;
}

eNextState SMTPC_read_auth_reply(SmtpOutMsg *SendMsg)
{
	/* Do an AUTH command if necessary */
	
	SMTP_DBG_READ();
	
	if (!SMTP_IS_STATE('2')) {
		if (SMTP_IS_STATE('4'))
			SMTP_VERROR(4)
		else 
			SMTP_VERROR(5)
	}
	return eSendReply;
}

eNextState SMTPC_send_FROM(SmtpOutMsg *SendMsg)
{
	/* previous command succeeded, now try the MAIL FROM: command */
	StrBufPrintf(SendMsg->IO.SendBuf.Buf,
		     "MAIL FROM:<%s>\r\n", 
		     SendMsg->envelope_from);

	SMTP_DBG_SEND();
	return eReadMessage;
}

eNextState SMTPC_read_FROM_reply(SmtpOutMsg *SendMsg)
{
	SMTP_DBG_READ();

	if (!SMTP_IS_STATE('2')) {
		if (SMTP_IS_STATE('4'))
			SMTP_VERROR(4)
		else 
			SMTP_VERROR(5)
	}
	return eSendReply;
}


eNextState SMTPC_send_RCPT(SmtpOutMsg *SendMsg)
{
	/* MAIL succeeded, now try the RCPT To: command */
	StrBufPrintf(SendMsg->IO.SendBuf.Buf,
		     "RCPT TO:<%s@%s>\r\n", 
		     SendMsg->user, 
		     SendMsg->node);

	SMTP_DBG_SEND();
	return eReadMessage;
}

eNextState SMTPC_read_RCPT_reply(SmtpOutMsg *SendMsg)
{
	SMTP_DBG_READ();

	if (!SMTP_IS_STATE('2')) {
		if (SMTP_IS_STATE('4')) 
			SMTP_VERROR(4)
		else 
			SMTP_VERROR(5)
	}
	return eSendReply;
}

eNextState SMTPC_send_DATAcmd(SmtpOutMsg *SendMsg)
{
	/* RCPT succeeded, now try the DATA command */
	StrBufPlain(SendMsg->IO.SendBuf.Buf,
		    HKEY("DATA\r\n"));

	SMTP_DBG_SEND();
	return eReadMessage;
}

eNextState SMTPC_read_DATAcmd_reply(SmtpOutMsg *SendMsg)
{
	SMTP_DBG_READ();

	if (!SMTP_IS_STATE('3')) {
		if (SMTP_IS_STATE('4')) 
			SMTP_VERROR(3)
		else 
			SMTP_VERROR(5)
	}
	return eSendReply;
}

eNextState SMTPC_send_data_body(SmtpOutMsg *SendMsg)
{
	StrBuf *Buf;
	/* If we reach this point, the server is expecting data.*/

	Buf = SendMsg->IO.SendBuf.Buf;
	SendMsg->IO.SendBuf.Buf = SendMsg->msgtext;
	SendMsg->msgtext = Buf;
	//// TODO timeout like that: (SendMsg->msg_size / 128) + 50);
	SendMsg->State ++;

	return eSendMore;
}

eNextState SMTPC_send_terminate_data_body(SmtpOutMsg *SendMsg)
{
	StrBuf *Buf;

	Buf = SendMsg->IO.SendBuf.Buf;
	SendMsg->IO.SendBuf.Buf = SendMsg->msgtext;
	SendMsg->msgtext = Buf;

	StrBufPlain(SendMsg->IO.SendBuf.Buf,
		    HKEY(".\r\n"));

	return eReadMessage;

}

eNextState SMTPC_read_data_body_reply(SmtpOutMsg *SendMsg)
{
	SMTP_DBG_READ();

	if (!SMTP_IS_STATE('2')) {
		if (SMTP_IS_STATE('4'))
			SMTP_VERROR(4)
		else 
			SMTP_VERROR(5)
	}

	/* We did it! */
	StrBufPlain(SendMsg->MyQEntry->StatusMessage, 
		    &ChrPtr(SendMsg->IO.RecvBuf.Buf)[4],
		    StrLength(SendMsg->IO.RecvBuf.Buf) - 4);
	SendMsg->MyQEntry->Status = 2;
	return eSendReply;
}

eNextState SMTPC_send_QUIT(SmtpOutMsg *SendMsg)
{
	StrBufPlain(SendMsg->IO.SendBuf.Buf,
		    HKEY("QUIT\r\n"));

	SMTP_DBG_SEND();
	return eReadMessage;
}

eNextState SMTPC_read_QUIT_reply(SmtpOutMsg *SendMsg)
{
	SMTP_DBG_READ();

	CtdlLogPrintf(CTDL_INFO, "SMTP client[%ld]: delivery to <%s> @ <%s> (%s) succeeded\n",
		      SendMsg->n, SendMsg->user, SendMsg->node, SendMsg->name);
	return eTerminateConnection;
}

eNextState SMTPC_read_dummy(SmtpOutMsg *SendMsg)
{
	return eSendReply;
}

eNextState SMTPC_send_dummy(SmtpOutMsg *SendMsg)
{
	return eReadMessage;
}

eNextState smtp_resolve_mx_done(void *data)
{/// VParsedDNSReply
	AsyncIO *IO = data;
	SmtpOutMsg * SendMsg = IO->Data;

	SendMsg->IO.SendBuf.Buf = NewStrBufPlain(NULL, 1024);
	SendMsg->IO.RecvBuf.Buf = NewStrBufPlain(NULL, 1024);
	SendMsg->IO.IOBuf = NewStrBuf();
	SendMsg->IO.ErrMsg = SendMsg->MyQEntry->StatusMessage;

	//// connect_one_smtpsrv_xamine_result
	SendMsg->CurrMX = SendMsg->AllMX = IO->VParsedDNSReply;
	//// TODO: should we remove the current ares context???
	connect_one_smtpsrv(SendMsg);
	return 0;
}



int resolve_mx_records(void *Ctx)
{
	SmtpOutMsg * SendMsg = Ctx;
/*//TMP
	SendMsg->IO.SendBuf.Buf = NewStrBufPlain(NULL, 1024);
	SendMsg->IO.RecvBuf.Buf = NewStrBufPlain(NULL, 1024);
	SendMsg->IO.IOBuf = NewStrBuf();
	SendMsg->IO.ErrMsg = SendMsg->MyQEntry->StatusMessage;

	InitEventIO(&SendMsg->IO, SendMsg, 
				    SMTP_C_DispatchReadDone, 
				    SMTP_C_DispatchWriteDone, 
				    SMTP_C_Terminate,
				    SMTP_C_Timeout,
				    SMTP_C_ConnFail,
				    SMTP_C_ReadServerStatus,
				    1);
				    return 0;
/// END TMP */
	if (!QueueQuery(ns_t_mx, 
			SendMsg->node, 
			&SendMsg->IO, 
			smtp_resolve_mx_done))
	{
		SendMsg->MyQEntry->Status = 5;
		StrBufPrintf(SendMsg->MyQEntry->StatusMessage, 
			     "No MX hosts found for <%s>", SendMsg->node);
		return 0; ///////TODO: abort!
	}
	return 0;
}

void smtp_try(OneQueItem *MyQItem, 
	      MailQEntry *MyQEntry, 
	      StrBuf *MsgText, 
	      int KeepMsgText) /* KeepMsgText allows us to use MsgText as ours. */
{
	SmtpOutMsg * SendMsg;

	SendMsg = (SmtpOutMsg *) malloc(sizeof(SmtpOutMsg));
	memset(SendMsg, 0, sizeof(SmtpOutMsg));
	SendMsg->IO.sock = (-1);
	SendMsg->n = MsgCount++;
	SendMsg->MyQEntry = MyQEntry;
	SendMsg->MyQItem = MyQItem;
	SendMsg->IO.Data = SendMsg;
	if (KeepMsgText)
		SendMsg->msgtext = MsgText;
	else 
		SendMsg->msgtext = NewStrBufDup(MsgText);

	smtp_resolve_recipients(SendMsg);

	QueueEventContext(SendMsg, 
			  &SendMsg->IO,
			  resolve_mx_records);


}



void NewMailQEntry(OneQueItem *Item)
{
	Item->Current = (MailQEntry*) malloc(sizeof(MailQEntry));
	memset(Item->Current, 0, sizeof(MailQEntry));

	if (Item->MailQEntries == NULL)
		Item->MailQEntries = NewHash(1, Flathash);
	Item->Current->n = GetCount(Item->MailQEntries);
	Put(Item->MailQEntries, IKEY(Item->Current->n), Item->Current, FreeMailQEntry);
}

void QItem_Handle_MsgID(OneQueItem *Item, StrBuf *Line, const char **Pos)
{
	Item->MessageID = StrBufExtractNext_int(Line, Pos, '|');
}

void QItem_Handle_EnvelopeFrom(OneQueItem *Item, StrBuf *Line, const char **Pos)
{
	if (Item->EnvelopeFrom == NULL)
		Item->EnvelopeFrom = NewStrBufPlain(NULL, StrLength(Line));
	StrBufExtract_NextToken(Item->EnvelopeFrom, Line, Pos, '|');
}

void QItem_Handle_BounceTo(OneQueItem *Item, StrBuf *Line, const char **Pos)
{
	if (Item->BounceTo == NULL)
		Item->BounceTo = NewStrBufPlain(NULL, StrLength(Line));
	StrBufExtract_NextToken(Item->BounceTo, Line, Pos, '|');
}

void QItem_Handle_Recipient(OneQueItem *Item, StrBuf *Line, const char **Pos)
{
	if (Item->Current == NULL)
		NewMailQEntry(Item);
	if (Item->Current->Recipient == NULL)
		Item->Current->Recipient =  NewStrBufPlain(NULL, StrLength(Line));
	StrBufExtract_NextToken(Item->Current->Recipient, Line, Pos, '|');
	Item->Current->Status = StrBufExtractNext_int(Line, Pos, '|');
	StrBufExtract_NextToken(Item->Current->StatusMessage, Line, Pos, '|');
	Item->Current = NULL; // TODO: is this always right?
}


void QItem_Handle_retry(OneQueItem *Item, StrBuf *Line, const char **Pos)
{
	if (Item->Current == NULL)
		NewMailQEntry(Item);
	if (Item->Current->Attempts[Item->Current->nAttempts].retry != 0)
		Item->Current->nAttempts++;
	if (Item->Current->nAttempts > MaxAttempts) {
		Item->FailNow = 1;
		return;
	}
	Item->Current->Attempts[Item->Current->nAttempts].retry = StrBufExtractNext_int(Line, Pos, '|');
}

void QItem_Handle_Attempted(OneQueItem *Item, StrBuf *Line, const char **Pos)
{
	if (Item->Current == NULL)
		NewMailQEntry(Item);
	if (Item->Current->Attempts[Item->Current->nAttempts].when != 0)
		Item->Current->nAttempts++;
	if (Item->Current->nAttempts > MaxAttempts) {
		Item->FailNow = 1;
		return;
	}
		
	Item->Current->Attempts[Item->Current->nAttempts].when = StrBufExtractNext_int(Line, Pos, '|');
	if (Item->Current->Attempts[Item->Current->nAttempts].when > Item->LastAttempt.when)
	{
		Item->LastAttempt.when = Item->Current->Attempts[Item->Current->nAttempts].when;
		Item->LastAttempt.retry = Item->Current->Attempts[Item->Current->nAttempts].retry * 2;
		if (Item->LastAttempt.retry > SMTP_RETRY_MAX)
			Item->LastAttempt.retry = SMTP_RETRY_MAX;
	}
}




/*
 * smtp_do_procmsg()
 *
 * Called by smtp_do_queue() to handle an individual message.
 */
void smtp_do_procmsg(long msgnum, void *userdata) {
	struct CtdlMessage *msg = NULL;
	char *instr = NULL;	
	StrBuf *PlainQItem;
	OneQueItem *MyQItem;
	char *pch;
	HashPos  *It;
	void *vQE;
	long len;
	const char *Key;

	CtdlLogPrintf(CTDL_DEBUG, "SMTP Queue: smtp_do_procmsg(%ld)\n", msgnum);
	///strcpy(envelope_from, "");

	msg = CtdlFetchMessage(msgnum, 1);
	if (msg == NULL) {
		CtdlLogPrintf(CTDL_ERR, "SMTP Queue: tried %ld but no such message!\n", msgnum);
		return;
	}

	pch = instr = msg->cm_fields['M'];

	/* Strip out the headers (no not amd any other non-instruction) line */
	while (pch != NULL) {
		pch = strchr(pch, '\n');
		if ((pch != NULL) && (*(pch + 1) == '\n')) {
			instr = pch + 2;
			pch = NULL;
		}
	}
	PlainQItem = NewStrBufPlain(instr, -1);
	CtdlFreeMessage(msg);
	MyQItem = DeserializeQueueItem(PlainQItem, msgnum);
	FreeStrBuf(&PlainQItem);

	if (MyQItem == NULL) {
		CtdlLogPrintf(CTDL_ERR, "SMTP Queue: Msg No %ld: already in progress!\n", msgnum);		
		return; /* s.b. else is already processing... */
	}

	/*
	 * Postpone delivery if we've already tried recently.
	 * /
	if (((time(NULL) - MyQItem->LastAttempt.when) < MyQItem->LastAttempt.retry) && (run_queue_now == 0)) {
		CtdlLogPrintf(CTDL_DEBUG, "SMTP client: Retry time not yet reached.\n");

		It = GetNewHashPos(MyQItem->MailQEntries, 0);
		citthread_mutex_lock(&ActiveQItemsLock);
		{
			GetHashPosFromKey(ActiveQItems, IKEY(MyQItem->MessageID), It);
			DeleteEntryFromHash(ActiveQItems, It);
		}
		citthread_mutex_unlock(&ActiveQItemsLock);
		////FreeQueItem(&MyQItem); TODO: DeleteEntryFromHash frees this?
		DeleteHashPos(&It);
		return;
	}// TODO: reenable me.*/

	/*
	 * Bail out if there's no actual message associated with this
	 */
	if (MyQItem->MessageID < 0L) {
		CtdlLogPrintf(CTDL_ERR, "SMTP Queue: no 'msgid' directive found!\n");
		It = GetNewHashPos(MyQItem->MailQEntries, 0);
		citthread_mutex_lock(&ActiveQItemsLock);
		{
			GetHashPosFromKey(ActiveQItems, IKEY(MyQItem->MessageID), It);
			DeleteEntryFromHash(ActiveQItems, It);
		}
		citthread_mutex_unlock(&ActiveQItemsLock);
		DeleteHashPos(&It);
		////FreeQueItem(&MyQItem); TODO: DeleteEntryFromHash frees this?
		return;
	}

	It = GetNewHashPos(MyQItem->MailQEntries, 0);
	while (GetNextHashPos(MyQItem->MailQEntries, It, &len, &Key, &vQE))
	{
		MailQEntry *ThisItem = vQE;
		CtdlLogPrintf(CTDL_DEBUG, "SMTP Queue: Task: <%s> %d\n", ChrPtr(ThisItem->Recipient), ThisItem->Active);
	}
	DeleteHashPos(&It);

	CountActiveQueueEntries(MyQItem);
	if (MyQItem->ActiveDeliveries > 0)
	{
		int i = 1;
		StrBuf *Msg = smtp_load_msg(MyQItem);
		It = GetNewHashPos(MyQItem->MailQEntries, 0);
		while ((i <= MyQItem->ActiveDeliveries) && 
		       (GetNextHashPos(MyQItem->MailQEntries, It, &len, &Key, &vQE)))
		{
			MailQEntry *ThisItem = vQE;
			if (ThisItem->Active == 1) {
				CtdlLogPrintf(CTDL_DEBUG, "SMTP Queue: Trying <%s>\n", ChrPtr(ThisItem->Recipient));
				smtp_try(MyQItem, ThisItem, Msg, (i == MyQItem->ActiveDeliveries));
				i++;
			}
		}
		DeleteHashPos(&It);
	}
	else 
	{
		It = GetNewHashPos(MyQItem->MailQEntries, 0);
		citthread_mutex_lock(&ActiveQItemsLock);
		{
			GetHashPosFromKey(ActiveQItems, IKEY(MyQItem->MessageID), It);
			DeleteEntryFromHash(ActiveQItems, It);
		}
		citthread_mutex_unlock(&ActiveQItemsLock);
		DeleteHashPos(&It);
		////FreeQueItem(&MyQItem); TODO: DeleteEntryFromHash frees this?

// TODO: bounce & delete?

	}
}


/*****************************************************************************/
/*                          SMTP UTILITY COMMANDS                            */
/*****************************************************************************/

void cmd_smtp(char *argbuf) {
	char cmd[64];
	char node[256];
	char buf[1024];
	int i;
	int num_mxhosts;

	if (CtdlAccessCheck(ac_aide)) return;

	extract_token(cmd, argbuf, 0, '|', sizeof cmd);

	if (!strcasecmp(cmd, "mx")) {
		extract_token(node, argbuf, 1, '|', sizeof node);
		num_mxhosts = getmx(buf, node);
		cprintf("%d %d MX hosts listed for %s\n",
			LISTING_FOLLOWS, num_mxhosts, node);
		for (i=0; i<num_mxhosts; ++i) {
			extract_token(node, buf, i, '|', sizeof node);
			cprintf("%s\n", node);
		}
		cprintf("000\n");
		return;
	}

	else if (!strcasecmp(cmd, "runqueue")) {
		run_queue_now = 1;
		cprintf("%d All outbound SMTP will be retried now.\n", CIT_OK);
		return;
	}

	else {
		cprintf("%d Invalid command.\n", ERROR + ILLEGAL_VALUE);
	}

}


/*
 * smtp_queue_thread()
 * 
 * Run through the queue sending out messages.
 */
void *smtp_queue_thread(void *arg) {
	int num_processed = 0;
	struct CitContext smtp_queue_CC;

	CtdlThreadSleep(10);

	CtdlFillSystemContext(&smtp_queue_CC, "SMTP Send");
	citthread_setspecific(MyConKey, (void *)&smtp_queue_CC);
	CtdlLogPrintf(CTDL_DEBUG, "smtp_queue_thread() initializing\n");

	while (!CtdlThreadCheckStop()) {
		
		CtdlLogPrintf(CTDL_INFO, "SMTP client: processing outbound queue\n");

		if (CtdlGetRoom(&CC->room, SMTP_SPOOLOUT_ROOM) != 0) {
			CtdlLogPrintf(CTDL_ERR, "Cannot find room <%s>\n", SMTP_SPOOLOUT_ROOM);
		}
		else {
			num_processed = CtdlForEachMessage(MSGS_ALL, 0L, NULL, SPOOLMIME, NULL, smtp_do_procmsg, NULL);
		}
		CtdlLogPrintf(CTDL_INFO, "SMTP client: queue run completed; %d messages processed\n", num_processed);
		CtdlThreadSleep(60);
	}

	CtdlClearSystemContext();
	return(NULL);
}


/*
 * Initialize the SMTP outbound queue
 */
void smtp_init_spoolout(void) {
	struct ctdlroom qrbuf;

	/*
	 * Create the room.  This will silently fail if the room already
	 * exists, and that's perfectly ok, because we want it to exist.
	 */
	CtdlCreateRoom(SMTP_SPOOLOUT_ROOM, 3, "", 0, 1, 0, VIEW_MAILBOX);

	/*
	 * Make sure it's set to be a "system room" so it doesn't show up
	 * in the <K>nown rooms list for Aides.
	 */
	if (CtdlGetRoomLock(&qrbuf, SMTP_SPOOLOUT_ROOM) == 0) {
		qrbuf.QRflags2 |= QR2_SYSTEM;
		CtdlPutRoomLock(&qrbuf);
	}
}


SMTPReadHandler ReadHandlers[eMaxSMTPC] = {
	SMTPC_read_greeting,
	SMTPC_read_EHLO_reply,
	SMTPC_read_HELO_reply,
	SMTPC_read_auth_reply,
	SMTPC_read_FROM_reply,
	SMTPC_read_RCPT_reply,
	SMTPC_read_DATAcmd_reply,
	SMTPC_read_dummy,
	SMTPC_read_data_body_reply,
	SMTPC_read_QUIT_reply
};

SMTPSendHandler SendHandlers[eMaxSMTPC] = {
	SMTPC_send_dummy, /* we don't send a greeting, the server does... */
	SMTPC_send_EHLO,
	STMPC_send_HELO,
	SMTPC_send_auth,
	SMTPC_send_FROM,
	SMTPC_send_RCPT,
	SMTPC_send_DATAcmd,
	SMTPC_send_data_body,
	SMTPC_send_terminate_data_body,
	SMTPC_send_QUIT
};

eNextState SMTP_C_Terminate(void *Data)
{
	SmtpOutMsg *pMsg = Data;
	FinalizeMessageSend(pMsg);
	return 0;
}

eNextState SMTP_C_Timeout(void *Data)
{
	SmtpOutMsg *pMsg = Data;
	FinalizeMessageSend(pMsg);
	return 0;
}

eNextState SMTP_C_ConnFail(void *Data)
{
	SmtpOutMsg *pMsg = Data;
	FinalizeMessageSend(pMsg);
	return 0;
}

eNextState SMTP_C_DispatchReadDone(void *Data)
{
	SmtpOutMsg *pMsg = Data;
	eNextState rc = ReadHandlers[pMsg->State](pMsg);
	pMsg->State++;
	return rc;
}

eNextState SMTP_C_DispatchWriteDone(void *Data)
{
	SmtpOutMsg *pMsg = Data;
	return SendHandlers[pMsg->State](pMsg);
	
}

#endif
CTDL_MODULE_INIT(smtp_eventclient)
{
#ifdef EXPERIMENTAL_SMTP_EVENT_CLIENT
	if (!threading)
	{
		ActiveQItems = NewHash(1, Flathash);
		citthread_mutex_init(&ActiveQItemsLock, NULL);

		QItemHandlers = NewHash(0, NULL);

		Put(QItemHandlers, HKEY("msgid"), QItem_Handle_MsgID, reference_free_handler);
		Put(QItemHandlers, HKEY("envelope_from"), QItem_Handle_EnvelopeFrom, reference_free_handler);
		Put(QItemHandlers, HKEY("retry"), QItem_Handle_retry, reference_free_handler);
		Put(QItemHandlers, HKEY("attempted"), QItem_Handle_Attempted, reference_free_handler);
		Put(QItemHandlers, HKEY("remote"), QItem_Handle_Recipient, reference_free_handler);
		Put(QItemHandlers, HKEY("bounceto"), QItem_Handle_BounceTo, reference_free_handler);
///submitted /TODO: flush qitemhandlers on exit


		smtp_init_spoolout();
		CtdlThreadCreate("SMTPEvent Send", CTDLTHREAD_BIGSTACK, smtp_queue_thread, NULL);

		CtdlRegisterProtoHook(cmd_smtp, "SMTP", "SMTP utility commands");
	}
#endif
	
	/* return our Subversion id for the Log */
	return "smtpeventclient";
}
