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

#include "smtpqueue.h"
#include "event_client.h"

HashList *QItemHandlers = NULL;

citthread_mutex_t ActiveQItemsLock;
HashList *ActiveQItems = NULL;

int MsgCount = 0;
int run_queue_now = 0;	/* Set to 1 to ignore SMTP send retry times */

void smtp_try(OneQueItem *MyQItem, 
	      MailQEntry *MyQEntry, 
	      StrBuf *MsgText, 
	      int KeepMsgText, /* KeepMsgText allows us to use MsgText as ours. */
	      int MsgCount);


void smtp_evq_cleanup(void)
{
	citthread_mutex_lock(&ActiveQItemsLock);
	DeleteHash(&QItemHandlers);
	DeleteHash(&ActiveQItems);
	citthread_mutex_unlock(&ActiveQItemsLock);
	citthread_mutex_destroy(&ActiveQItemsLock);
}

int DecreaseQReference(OneQueItem *MyQItem)
{
	int IDestructQueItem;

	citthread_mutex_lock(&ActiveQItemsLock);
	MyQItem->ActiveDeliveries--;
	IDestructQueItem = MyQItem->ActiveDeliveries == 0;
	citthread_mutex_unlock(&ActiveQItemsLock);
	return IDestructQueItem;
}

void RemoveQItem(OneQueItem *MyQItem)
{
	HashPos  *It;

	It = GetNewHashPos(MyQItem->MailQEntries, 0);
	citthread_mutex_lock(&ActiveQItemsLock);
	{
		GetHashPosFromKey(ActiveQItems, IKEY(MyQItem->MessageID), It);
		DeleteEntryFromHash(ActiveQItems, It);
	}
	citthread_mutex_unlock(&ActiveQItemsLock);
	DeleteHashPos(&It);
}



void FreeMailQEntry(void *qv)
{
	MailQEntry *Q = qv;
	FreeStrBuf(&Q->Recipient);
	FreeStrBuf(&Q->StatusMessage);
	free(Q);
}
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

	StrBufAppendBufPlain(QMessage, HKEY("\nsubmitted|"), 0);
	StrBufAppendPrintf(QMessage, "%ld", MyQItem->Submitted);

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
			/* TODO: most probably there is just one retry/attempted per message! */
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





void NewMailQEntry(OneQueItem *Item)
{
	Item->Current = (MailQEntry*) malloc(sizeof(MailQEntry));
	memset(Item->Current, 0, sizeof(MailQEntry));

	if (Item->MailQEntries == NULL)
		Item->MailQEntries = NewHash(1, Flathash);
	Item->Current->StatusMessage = NewStrBuf();
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


void QItem_Handle_Submitted(OneQueItem *Item, StrBuf *Line, const char **Pos)
{
	Item->Submitted = atol(*Pos);

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



/**
 * this one has to have the context for loading the message via the redirect buffer...
 */
StrBuf *smtp_load_msg(OneQueItem *MyQItem, int n)
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



/*
 * smtp_do_bounce() is caled by smtp_do_procmsg() to scan a set of delivery
 * instructions for "5" codes (permanent fatal errors) and produce/deliver
 * a "bounce" message (delivery status notification).
 */
void smtpq_do_bounce(OneQueItem *MyQItem, StrBuf *OMsgTxt) 
{
	static int seq = 0;

	struct CtdlMessage *bmsg = NULL;
	StrBuf *boundary;
	StrBuf *Msg = NULL; 
	StrBuf *BounceMB;
	struct recptypes *valid;
	
	HashPos *It;
	void *vQE;
	long len;
	const char *Key;

	int successful_bounce = 0;
	int num_bounces = 0;
	int give_up = 0;

	CtdlLogPrintf(CTDL_DEBUG, "smtp_do_bounce() called\n");

	if ( (ev_time() - MyQItem->Submitted) > SMTP_GIVE_UP ) {
		give_up = 1;/// TODO: replace time by libevq timer get
	}

	/*
	 * Now go through the instructions checking for stuff.
	 */
	It = GetNewHashPos(MyQItem->MailQEntries, 0);
	while (GetNextHashPos(MyQItem->MailQEntries, It, &len, &Key, &vQE))
	{
		MailQEntry *ThisItem = vQE;
		if ((ThisItem->Status == 5) || /* failed now? */
		    ((give_up == 1) && (ThisItem->Status != 2))) /* giving up after failed attempts... */
		{
			if (num_bounces == 0)
				Msg = NewStrBufPlain(NULL, 1024);
			++num_bounces;
	
			StrBufAppendBuf(Msg, ThisItem->Recipient, 0);
			StrBufAppendBufPlain(Msg, HKEY(": "), 0);
			StrBufAppendBuf(Msg, ThisItem->StatusMessage, 0);
			StrBufAppendBufPlain(Msg, HKEY("\r\n"), 0);
		}
	}
	DeleteHashPos(&It);

	/* Deliver the bounce if there's anything worth mentioning */
	CtdlLogPrintf(CTDL_DEBUG, "num_bounces = %d\n", num_bounces);

	if (num_bounces == 0) {
		FreeStrBuf(&Msg);
		return;
	}

	boundary = NewStrBufPlain(HKEY("=_Citadel_Multipart_"));
	StrBufAppendPrintf(boundary, "%s_%04x%04x", config.c_fqdn, getpid(), ++seq);

	/* Start building our bounce message; go shopping for memory first. */
	BounceMB = NewStrBufPlain(NULL, 
				  1024 + /* mime stuff.... */
				  StrLength(Msg) +  /* the bounce information... */
				  StrLength(OMsgTxt)); /* the original message */
	if (BounceMB == NULL) {
		FreeStrBuf(&boundary);
		CtdlLogPrintf(CTDL_ERR, "Failed to alloc() bounce message.\n");

		return;
	}

	bmsg = (struct CtdlMessage *) malloc(sizeof(struct CtdlMessage));
	if (bmsg == NULL) {
		FreeStrBuf(&boundary);
		FreeStrBuf(&BounceMB);
		CtdlLogPrintf(CTDL_ERR, "Failed to alloc() bounce message.\n");

		return;
	}
	memset(bmsg, 0, sizeof(struct CtdlMessage));


	StrBufAppendBufPlain(BounceMB, HKEY("Content-type: multipart/mixed; boundary=\""), 0);
	StrBufAppendBuf(BounceMB, boundary, 0);
        StrBufAppendBufPlain(BounceMB, HKEY("\"\r\n"), 0);
	StrBufAppendBufPlain(BounceMB, HKEY("MIME-Version: 1.0\r\n"), 0);
	StrBufAppendBufPlain(BounceMB, HKEY("X-Mailer: " CITADEL "\r\n"), 0);
        StrBufAppendBufPlain(BounceMB, HKEY("\r\nThis is a multipart message in MIME format.\r\n\r\n"), 0);
        StrBufAppendBufPlain(BounceMB, HKEY("--"), 0);
        StrBufAppendBuf(BounceMB, boundary, 0);
	StrBufAppendBufPlain(BounceMB, HKEY("\r\n"), 0);
        StrBufAppendBufPlain(BounceMB, HKEY("Content-type: text/plain\r\n\r\n"), 0);

	if (give_up) 
		StrBufAppendBufPlain(
			BounceMB, 
			HKEY(
				"A message you sent could not be delivered to some or all of its recipients\n"
				"due to prolonged unavailability of its destination(s).\n"
				"Giving up on the following addresses:\n\n"
				), 0);
	else 
		StrBufAppendBufPlain(
			BounceMB, 
			HKEY(
				"A message you sent could not be delivered to some or all of its recipients.\n"
				"The following addresses were undeliverable:\n\n"
				), 0);

	StrBufAppendBuf(BounceMB, Msg, 0);
	FreeStrBuf(&Msg);

	/* Attach the original message */
	StrBufAppendBufPlain(BounceMB, HKEY("--"), 0);
	StrBufAppendBuf(BounceMB, boundary, 0);
	StrBufAppendBufPlain(BounceMB, HKEY("\r\n"), 0);
	StrBufAppendBufPlain(BounceMB, HKEY("Content-type: message/rfc822\r\n"), 0);
	StrBufAppendBufPlain(BounceMB, HKEY("Content-Transfer-Encoding: 7bit\r\n"), 0);
	StrBufAppendBufPlain(BounceMB, HKEY("Content-Disposition: inline\r\n"), 0);
	StrBufAppendBufPlain(BounceMB, HKEY("\r\n"), 0);
	StrBufAppendBuf(BounceMB, OMsgTxt, 0);

	/* Close the multipart MIME scope */
        StrBufAppendBufPlain(BounceMB, HKEY("--"), 0);
	StrBufAppendBuf(BounceMB, boundary, 0);
	StrBufAppendBufPlain(BounceMB, HKEY("--\r\n"), 0);



        bmsg->cm_magic = CTDLMESSAGE_MAGIC;
        bmsg->cm_anon_type = MES_NORMAL;
        bmsg->cm_format_type = FMT_RFC822;

        bmsg->cm_fields['O'] = strdup(MAILROOM);
        bmsg->cm_fields['A'] = strdup("Citadel");
        bmsg->cm_fields['N'] = strdup(config.c_nodename);
        bmsg->cm_fields['U'] = strdup("Delivery Status Notification (Failure)");
	bmsg->cm_fields['M'] = SmashStrBuf(&BounceMB);

	/* First try the user who sent the message */
	if (StrLength(MyQItem->BounceTo) == 0) 
		CtdlLogPrintf(CTDL_ERR, "No bounce address specified\n");
	else
		CtdlLogPrintf(CTDL_DEBUG, "bounce to user? <%s>\n", ChrPtr(MyQItem->BounceTo));

	/* Can we deliver the bounce to the original sender? */
	valid = validate_recipients(ChrPtr(MyQItem->BounceTo), NULL, 0);
	if ((valid != NULL) && (valid->num_error == 0)) {
		CtdlSubmitMsg(bmsg, valid, "", QP_EADDR);
		successful_bounce = 1;
	}

	/* If not, post it in the Aide> room */
	if (successful_bounce == 0) {
		CtdlSubmitMsg(bmsg, NULL, config.c_aideroom, QP_EADDR);
	}

	/* Free up the memory we used */
	free_recipients(valid);
	FreeStrBuf(&boundary);
	CtdlFreeMessage(bmsg);
	CtdlLogPrintf(CTDL_DEBUG, "Done processing bounces\n");
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
		int n = MsgCount++;
		int i = 1;
		StrBuf *Msg = smtp_load_msg(MyQItem, n);
		It = GetNewHashPos(MyQItem->MailQEntries, 0);
		while ((i <= MyQItem->ActiveDeliveries) && 
		       (GetNextHashPos(MyQItem->MailQEntries, It, &len, &Key, &vQE)))
		{
			MailQEntry *ThisItem = vQE;
			if (ThisItem->Active == 1) {
				if (i > 1) n = MsgCount++;
				CtdlLogPrintf(CTDL_DEBUG, "SMTP Queue: Trying <%s>\n", ChrPtr(ThisItem->Recipient));
				smtp_try(MyQItem, ThisItem, Msg, (i == MyQItem->ActiveDeliveries), n);
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




CTDL_MODULE_INIT(smtp_queu)
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
		Put(QItemHandlers, HKEY("submitted"), QItem_Handle_Submitted, reference_free_handler);
////TODO: flush qitemhandlers on exit
		smtp_init_spoolout();

		CtdlRegisterCleanupHook(smtp_evq_cleanup);
		CtdlThreadCreate("SMTPEvent Send", CTDLTHREAD_BIGSTACK, smtp_queue_thread, NULL);

		CtdlRegisterProtoHook(cmd_smtp, "SMTP", "SMTP utility commands");
	}
#endif
	
	/* return our Subversion id for the Log */
	return "smtpeventclient";
}
