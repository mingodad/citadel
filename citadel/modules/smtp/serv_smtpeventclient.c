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
#include "smtpqueue.h"

#ifdef EXPERIMENTAL_SMTP_EVENT_CLIENT
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

const long SMTP_C_ConnTimeout = 300; /* wail 5 minutes for connections... */
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

	ares_free_data(Msg->AllMX);
	
	FreeStrBuf(&Msg->msgtext);
	FreeAsyncIOContents(&Msg->IO);
	free(Msg);
}

eNextState SMTP_C_Timeout(void *Data);
eNextState SMTP_C_ConnFail(void *Data);
eNextState SMTP_C_DispatchReadDone(void *Data);
eNextState SMTP_C_DispatchWriteDone(void *Data);
eNextState SMTP_C_Terminate(void *Data);
eReadState SMTP_C_ReadServerStatus(AsyncIO *IO);

typedef eNextState (*SMTPReadHandler)(SmtpOutMsg *Msg);
typedef eNextState (*SMTPSendHandler)(SmtpOutMsg *Msg);


#define SMTP_ERROR(WHICH_ERR, ERRSTR) do {\
		SendMsg->MyQEntry->Status = WHICH_ERR; \
		StrBufAppendBufPlain(SendMsg->MyQEntry->StatusMessage, HKEY(ERRSTR), 0); \
		return eAbort; } \
	while (0)

#define SMTP_VERROR(WHICH_ERR) do {\
		SendMsg->MyQEntry->Status = WHICH_ERR; \
		StrBufPlain(SendMsg->MyQEntry->StatusMessage, \
			    ChrPtr(SendMsg->IO.IOBuf) + 4, \
			    StrLength(SendMsg->IO.IOBuf) - 4); \
		return eAbort; } \
	while (0)

#define SMTP_IS_STATE(WHICH_STATE) (ChrPtr(SendMsg->IO.IOBuf)[0] == WHICH_STATE)

#define SMTP_DBG_SEND() CtdlLogPrintf(CTDL_DEBUG, "SMTP client[%ld]: > %s\n", SendMsg->n, ChrPtr(SendMsg->IO.IOBuf))
#define SMTP_DBG_READ() CtdlLogPrintf(CTDL_DEBUG, "SMTP client[%ld]: < %s\n", SendMsg->n, ChrPtr(SendMsg->IO.IOBuf))


void FinalizeMessageSend(SmtpOutMsg *Msg)
{
	if (DecreaseQReference(Msg->MyQItem)) 
	{
		int nRemain;
		StrBuf *MsgData;

		nRemain = CountActiveQueueEntries(Msg->MyQItem);

		MsgData = SerializeQueueItem(Msg->MyQItem);
		/*
		 * Uncompleted delivery instructions remain, so delete the old
		 * instructions and replace with the updated ones.
		 */
		CtdlDeleteMessages(SMTP_SPOOLOUT_ROOM, &Msg->MyQItem->QueMsgID, 1, "");
		smtpq_do_bounce(Msg->MyQItem,
			       Msg->msgtext); 
		if (nRemain > 0) {
			struct CtdlMessage *msg;
			msg = malloc(sizeof(struct CtdlMessage));
			memset(msg, 0, sizeof(struct CtdlMessage));
			msg->cm_magic = CTDLMESSAGE_MAGIC;
			msg->cm_anon_type = MES_NORMAL;
			msg->cm_format_type = FMT_RFC822;
			msg->cm_fields['M'] = SmashStrBuf(&MsgData);
			/* Generate 'bounce' messages */
			smtp_do_bounce(msg->cm_fields['M'],
				       Msg->msgtext); 

			CtdlSubmitMsg(msg, NULL, SMTP_SPOOLOUT_ROOM, QP_EADDR);
			CtdlFreeMessage(msg);
		}
		else 
			CtdlDeleteMessages(SMTP_SPOOLOUT_ROOM, &Msg->MyQItem->MessageID, 1, "");

		RemoveQItem(Msg->MyQItem);
	}
	
	close(Msg->IO.sock);
	DeleteSmtpOutMsg(Msg);
}




void get_one_mx_host_ip_done(void *Ctx, 
			       int status,
			       int timeouts,
			       struct hostent *hostent)
{
	AsyncIO *IO = Ctx;
	SmtpOutMsg *SendMsg = IO->Data;
	if ((status == ARES_SUCCESS) && (hostent != NULL) ) {
		CtdlLogPrintf(CTDL_DEBUG, 
			      "SMTP client[%ld]: connecting to %s [ip]: %d ...\n", 
			      SendMsg->n, 
			      SendMsg->mx_host, 
			      SendMsg->IO.dport);

		SendMsg->MyQEntry->Status = 5; 
		StrBufPrintf(SendMsg->MyQEntry->StatusMessage, 
			     "Timeout while connecting %s", 
			     SendMsg->mx_host);

		SendMsg->IO.HEnt = hostent;
		InitEventIO(IO, SendMsg, 
			    SMTP_C_DispatchReadDone, 
			    SMTP_C_DispatchWriteDone, 
			    SMTP_C_Terminate,
			    SMTP_C_Timeout,
			    SMTP_C_ConnFail,
			    SMTP_C_ReadServerStatus,
			    SMTP_C_ConnTimeout, 
			    SMTP_C_ReadTimeouts[0],
			    1);

	}
}

const unsigned short DefaultMXPort = 25;
void get_one_mx_host_ip(SmtpOutMsg *SendMsg)
{
	//char *endpart;
	//char buf[SIZ];

	SendMsg->IO.dport = DefaultMXPort;


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
		      "SMTP client[%ld]: looking up %s : %d ...\n", 
		      SendMsg->n, 
		      SendMsg->mx_host);

	ares_gethostbyname(SendMsg->IO.DNSChannel,
			   SendMsg->mx_host,   
			   AF_INET6, /* it falls back to ipv4 in doubt... */
			   get_one_mx_host_ip_done,
			   &SendMsg->IO);
}


eNextState smtp_resolve_mx_done(void *data)
{
	AsyncIO *IO = data;
	SmtpOutMsg * SendMsg = IO->Data;

	SendMsg->IO.SendBuf.Buf = NewStrBufPlain(NULL, 1024);
	SendMsg->IO.RecvBuf.Buf = NewStrBufPlain(NULL, 1024);
	SendMsg->IO.IOBuf = NewStrBuf();
	SendMsg->IO.ErrMsg = SendMsg->MyQEntry->StatusMessage;

	SendMsg->CurrMX = SendMsg->AllMX = IO->VParsedDNSReply;
	//// TODO: should we remove the current ares context???
	get_one_mx_host_ip(SendMsg);
	return 0;
}


int resolve_mx_records(void *Ctx)
{
	SmtpOutMsg * SendMsg = Ctx;

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


int smtp_resolve_recipients(SmtpOutMsg *SendMsg)
{
	const char *ptr;
	char buf[1024];
	int scan_done;
	int lp, rp;
	int i;

	if ((SendMsg==NULL) || 
	    (SendMsg->MyQEntry == NULL) || 
	    (StrLength(SendMsg->MyQEntry->Recipient) == 0)) {
		return 0;
	}

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

	return 1;
}



void smtp_try(OneQueItem *MyQItem, 
	      MailQEntry *MyQEntry, 
	      StrBuf *MsgText, 
	      int KeepMsgText,  /* KeepMsgText allows us to use MsgText as ours. */
	      int MsgCount)
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

	if (smtp_resolve_recipients(SendMsg)) {
		QueueEventContext(SendMsg, 
				  &SendMsg->IO,
				  resolve_mx_records);
	}
	else {
		if ((SendMsg==NULL) || 
		    (SendMsg->MyQEntry == NULL)) {
			SendMsg->MyQEntry->Status = 5;
			StrBufPlain(SendMsg->MyQEntry->StatusMessage, 
				    HKEY("Invalid Recipient!"));
		}
		FinalizeMessageSend(SendMsg);
	}
}





/*****************************************************************************/
/*                     SMTP CLIENT STATE CALLBACKS                           */
/*****************************************************************************/
eNextState SMTPC_read_greeting(SmtpOutMsg *SendMsg)
{
	/* Process the SMTP greeting from the server */
	SMTP_DBG_READ();

	if (!SMTP_IS_STATE('2')) {
		if (SMTP_IS_STATE('4')) 
			SMTP_VERROR(4);
		else 
			SMTP_VERROR(5);
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
			SMTP_VERROR(4);
		else 
			SMTP_VERROR(5);
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
			SMTP_VERROR(4);
		else 
			SMTP_VERROR(5);
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
			SMTP_VERROR(4);
		else 
			SMTP_VERROR(5);
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
			SMTP_VERROR(4);
		else 
			SMTP_VERROR(5);
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
			SMTP_VERROR(3);
		else 
			SMTP_VERROR(5);
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
			SMTP_VERROR(4);
		else 
			SMTP_VERROR(5);
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


/*****************************************************************************/
/*                     SMTP CLIENT DISPATCHER                                */
/*****************************************************************************/
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


/*****************************************************************************/
/*                     SMTP CLIENT ERROR CATCHERS                            */
/*****************************************************************************/
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


/**
 * @brief lineread Handler; understands when to read more SMTP lines, and when this is a one-lined reply.
 */
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


#endif
CTDL_MODULE_INIT(smtp_eventclient)
{
	return "smtpeventclient";
}
