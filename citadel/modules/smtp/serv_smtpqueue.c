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
 * Copyright (c) 1998-2012 by the citadel.org team
 *
 *  This program is open source software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 *  
 *  
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  
 *  
 *  
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
#include "smtp_clienthandlers.h"
#include "event_client.h"


struct CitContext smtp_queue_CC;
pthread_mutex_t ActiveQItemsLock;
HashList *ActiveQItems  = NULL;
HashList *QItemHandlers = NULL;
const unsigned short DefaultMXPort = 25;
int max_sessions_for_outbound_smtp = 500; /* how many sessions might be active till we stop adding more smtp jobs */
int ndelay_count = 50; /* every n queued messages we will sleep... */
int delay_msec = 5000; /* this many seconds. */

static const long MaxRetry = SMTP_RETRY_INTERVAL * 2 * 2 * 2 * 2 * 2 * 2 * 2 * 2 * 2 * 2 * 2 * 2 * 2 * 2;
int MsgCount            = 0;
int run_queue_now       = 0;	/* Set to 1 to ignore SMTP send retry times */

void RegisterQItemHandler(const char *Key, long Len, QItemHandler H)
{
	QItemHandlerStruct *HS = (QItemHandlerStruct*)malloc(sizeof(QItemHandlerStruct));
	HS->H = H;
	Put(QItemHandlers, Key, Len, HS, NULL);
}



void smtp_try_one_queue_entry(OneQueItem *MyQItem,
			      MailQEntry *MyQEntry,
			      StrBuf *MsgText,
/* KeepMsgText allows us to use MsgText as ours. */
			      int KeepMsgText,
			      int MsgCount,
			      ParsedURL *RelayUrls);


void smtp_evq_cleanup(void)
{

	pthread_mutex_lock(&ActiveQItemsLock);
	DeleteHash(&QItemHandlers);
	DeleteHash(&ActiveQItems);
	pthread_mutex_unlock(&ActiveQItemsLock);
	pthread_setspecific(MyConKey, (void *)&smtp_queue_CC);
/*	citthread_mutex_destroy(&ActiveQItemsLock); TODO */
}

int DecreaseQReference(OneQueItem *MyQItem)
{
	int IDestructQueItem;

	pthread_mutex_lock(&ActiveQItemsLock);
	MyQItem->ActiveDeliveries--;
	IDestructQueItem = MyQItem->ActiveDeliveries == 0;
	pthread_mutex_unlock(&ActiveQItemsLock);
	return IDestructQueItem;
}

void DecreaseShutdownDeliveries(OneQueItem *MyQItem)
{
	pthread_mutex_lock(&ActiveQItemsLock);
	MyQItem->NotYetShutdownDeliveries--;
	pthread_mutex_unlock(&ActiveQItemsLock);
}

int GetShutdownDeliveries(OneQueItem *MyQItem)
{
	int DestructNow;

	pthread_mutex_lock(&ActiveQItemsLock);
	DestructNow = MyQItem->ActiveDeliveries == 0;
	pthread_mutex_unlock(&ActiveQItemsLock);
	return DestructNow;
}
void RemoveQItem(OneQueItem *MyQItem)
{
	long len;
	const char* Key;
	void *VData;
	HashPos  *It;

	pthread_mutex_lock(&ActiveQItemsLock);
	It = GetNewHashPos(ActiveQItems, 0);
	if (GetHashPosFromKey(ActiveQItems, LKEY(MyQItem->MessageID), It))
		DeleteEntryFromHash(ActiveQItems, It);
	else
	{
		SMTPC_syslog(LOG_WARNING,
			     "unable to find QItem with ID[%ld]",
			     MyQItem->MessageID);
		while (GetNextHashPos(ActiveQItems, It, &len, &Key, &VData))
			SMTPC_syslog(LOG_WARNING,
				     "have_: ID[%ld]",
				     ((OneQueItem *)VData)->MessageID);
	}
	pthread_mutex_unlock(&ActiveQItemsLock);
	DeleteHashPos(&It);
}


void FreeMailQEntry(void *qv)
{
	MailQEntry *Q = qv;
/*
	SMTPC_syslog(LOG_DEBUG, "---------------%s--------------", __FUNCTION__);
	cit_backtrace();
*/
	FreeStrBuf(&Q->Recipient);
	FreeStrBuf(&Q->StatusMessage);
	FreeStrBuf(&Q->AllStatusMessages);
	memset(Q, 0, sizeof(MailQEntry));
	free(Q);
}
void FreeQueItem(OneQueItem **Item)
{
/*
	SMTPC_syslog(LOG_DEBUG, "---------------%s--------------", __FUNCTION__);
	cit_backtrace();
*/
	DeleteHash(&(*Item)->MailQEntries);
	FreeStrBuf(&(*Item)->EnvelopeFrom);
	FreeStrBuf(&(*Item)->BounceTo);
	FreeStrBuf(&(*Item)->SenderRoom);
	FreeURL(&(*Item)->URL);
	memset(*Item, 0, sizeof(OneQueItem));
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
int CheckQEntryActive(MailQEntry *ThisItem)
{
	if ((ThisItem->Status == 0) ||
	    (ThisItem->Status == 3) ||
	    (ThisItem->Status == 4))
	{
		return 1;
	}
	else
		return 0;
}
int CheckQEntryIsBounce(MailQEntry *ThisItem)
{
	if ((ThisItem->Status == 3) ||
	    (ThisItem->Status == 4) ||
	    (ThisItem->Status == 5))
	{
		return 1;
	}
	else
		return 0;
}	

int CountActiveQueueEntries(OneQueItem *MyQItem, int before)
{
	HashPos  *It;
	long len;
	long ActiveDeliveries;
	const char *Key;
	void *vQE;

	ActiveDeliveries = 0;
	It = GetNewHashPos(MyQItem->MailQEntries, 0);
	while (GetNextHashPos(MyQItem->MailQEntries, It, &len, &Key, &vQE))
	{
		int Active;
		MailQEntry *ThisItem = vQE;

		if (CheckQEntryActive(ThisItem))
		{
			ActiveDeliveries++;
			Active = 1;
		}
		else
			Active = 0;
		if (before)
			ThisItem->Active = Active;
		else
			ThisItem->StillActive = Active;
	}
	DeleteHashPos(&It);
	return ActiveDeliveries;
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
	Item->Retry = SMTP_RETRY_INTERVAL;
	Item->MessageID = -1;
	Item->QueMsgID = QueMsgID;

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
			QItemHandlerStruct *HS;
			HS = (QItemHandlerStruct*) vHandler;
			HS->H(Item, Line, &pItemPart);
		}
	}
	FreeStrBuf(&Line);
	FreeStrBuf(&Token);

	if (Item->Retry >= MaxRetry)
		Item->FailNow = 1;

	pthread_mutex_lock(&ActiveQItemsLock);
	if (GetHash(ActiveQItems,
		    LKEY(Item->MessageID),
		    &v))
	{
		/* WHOOPS. somebody else is already working on this. */
		pthread_mutex_unlock(&ActiveQItemsLock);
		FreeQueItem(&Item);
		return NULL;
	}
	else {
		/* mark our claim on this. */
		Put(ActiveQItems,
		    LKEY(Item->MessageID),
		    Item,
		    HFreeQueItem);
		pthread_mutex_unlock(&ActiveQItemsLock);
	}

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
//	"attempted|%ld\n"  "retry|%ld\n",, (long)time(NULL), (long)retry );
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

	if (StrLength(MyQItem->SenderRoom) > 0) {
		StrBufAppendBufPlain(QMessage, HKEY("\nsource_room|"), 0);
		StrBufAppendBuf(QMessage, MyQItem->SenderRoom, 0);
	}

	StrBufAppendBufPlain(QMessage, HKEY("\nretry|"), 0);
	StrBufAppendPrintf(QMessage, "%ld",
			   MyQItem->Retry);

	StrBufAppendBufPlain(QMessage, HKEY("\nattempted|"), 0);
	StrBufAppendPrintf(QMessage, "%ld",
			   time(NULL) /*ctdl_ev_now()*/ + MyQItem->Retry);

	It = GetNewHashPos(MyQItem->MailQEntries, 0);
	while (GetNextHashPos(MyQItem->MailQEntries, It, &len, &Key, &vQE))
	{
		MailQEntry *ThisItem = vQE;

		StrBufAppendBufPlain(QMessage, HKEY("\nremote|"), 0);
		StrBufAppendBuf(QMessage, ThisItem->Recipient, 0);
		StrBufAppendBufPlain(QMessage, HKEY("|"), 0);
		StrBufAppendPrintf(QMessage, "%d", ThisItem->Status);
		StrBufAppendBufPlain(QMessage, HKEY("|"), 0);
		if (ThisItem->AllStatusMessages != NULL)
			StrBufAppendBuf(QMessage, ThisItem->AllStatusMessages, 0);
		else
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
	/* alocate big buffer so we won't get problems reallocating later. */
	Item->Current->StatusMessage = NewStrBufPlain(NULL, SIZ);
	Item->Current->n = GetCount(Item->MailQEntries);
	Put(Item->MailQEntries,
	    IKEY(Item->Current->n),
	    Item->Current,
	    FreeMailQEntry);
}

void QItem_Handle_MsgID(OneQueItem *Item, StrBuf *Line, const char **Pos)
{
	Item->MessageID = StrBufExtractNext_long(Line, Pos, '|');
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

void QItem_Handle_SenderRoom(OneQueItem *Item, StrBuf *Line, const char **Pos)
{
	if (Item->SenderRoom == NULL)
		Item->SenderRoom = NewStrBufPlain(NULL, StrLength(Line));
	StrBufExtract_NextToken(Item->SenderRoom, Line, Pos, '|');
}

void QItem_Handle_Recipient(OneQueItem *Item, StrBuf *Line, const char **Pos)
{
	if (Item->Current == NULL)
		NewMailQEntry(Item);
	if (Item->Current->Recipient == NULL)
		Item->Current->Recipient=NewStrBufPlain(NULL, StrLength(Line));
	StrBufExtract_NextToken(Item->Current->Recipient, Line, Pos, '|');
	Item->Current->Status = StrBufExtractNext_int(Line, Pos, '|');
	StrBufExtract_NextToken(Item->Current->StatusMessage, Line, Pos, '|');
	Item->Current = NULL; // TODO: is this always right?
}


void QItem_Handle_retry(OneQueItem *Item, StrBuf *Line, const char **Pos)
{
	Item->Retry =
		StrBufExtractNext_int(Line, Pos, '|');
	if (Item->Retry == 0)
		Item->Retry = SMTP_RETRY_INTERVAL;
	else
		Item->Retry *= 2;
}


void QItem_Handle_Submitted(OneQueItem *Item, StrBuf *Line, const char **Pos)
{
	Item->Submitted = atol(*Pos);

}

void QItem_Handle_Attempted(OneQueItem *Item, StrBuf *Line, const char **Pos)
{
	Item->ReattemptWhen = StrBufExtractNext_int(Line, Pos, '|');
}



/**
 * this one has to have the context for loading the message via the redirect buffer...
 */
StrBuf *smtp_load_msg(OneQueItem *MyQItem, int n, char **Author, char **Address)
{
	CitContext *CCC=CC;
	StrBuf *SendMsg;

	CCC->redirect_buffer = NewStrBufPlain(NULL, SIZ);
	CtdlOutputMsg(MyQItem->MessageID,
		      MT_RFC822, HEADERS_ALL,
		      0, 1, NULL,
		      (ESC_DOT|SUPPRESS_ENV_TO),
		      Author,
		      Address);

	SendMsg = CCC->redirect_buffer;
	CCC->redirect_buffer = NULL;
	if ((StrLength(SendMsg) > 0) &&
	    ChrPtr(SendMsg)[StrLength(SendMsg) - 1] != '\n') {
		SMTPC_syslog(LOG_WARNING,
			     "[%d] Possible problem: message did not "
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
void smtpq_do_bounce(OneQueItem *MyQItem, StrBuf *OMsgTxt, ParsedURL *Relay)
{
	static int seq = 0;
	
	struct CtdlMessage *bmsg = NULL;
	StrBuf *boundary;
	StrBuf *Msg = NULL;
	StrBuf *BounceMB;
	struct recptypes *valid;
	time_t now;

	HashPos *It;
	void *vQE;
	long len;
	const char *Key;

	int first_attempt = 0;
	int successful_bounce = 0;
	int num_bounces = 0;
	int give_up = 0;

	SMTPCM_syslog(LOG_DEBUG, "smtp_do_bounce() called\n");

	if (MyQItem->SendBounceMail == 0)
		return;

	now = time (NULL); //ev_time();

	if ( (now - MyQItem->Submitted) > SMTP_GIVE_UP ) {
		give_up = 1;
	}

	if (MyQItem->Retry == SMTP_RETRY_INTERVAL) {
		first_attempt = 1;
	}

	/*
	 * Now go through the instructions checking for stuff.
	 */
	Msg = NewStrBufPlain(NULL, 1024);
	It = GetNewHashPos(MyQItem->MailQEntries, 0);
	while (GetNextHashPos(MyQItem->MailQEntries, It, &len, &Key, &vQE))
	{
		MailQEntry *ThisItem = vQE;
		if ((ThisItem->Active && (ThisItem->Status == 5)) || /* failed now? */
		    ((give_up == 1) && (ThisItem->Status != 2)) ||
		    ((first_attempt == 1) && (ThisItem->Status != 2)))
			/* giving up after failed attempts... */
		{
			++num_bounces;

			StrBufAppendBufPlain(Msg, HKEY(" "), 0);
			StrBufAppendBuf(Msg, ThisItem->Recipient, 0);
			StrBufAppendBufPlain(Msg, HKEY(": "), 0);
			if (ThisItem->AllStatusMessages != NULL)
				StrBufAppendBuf(Msg, ThisItem->AllStatusMessages, 0);
			else
				StrBufAppendBuf(Msg, ThisItem->StatusMessage, 0);
			StrBufAppendBufPlain(Msg, HKEY("\r\n"), 0);
		}
	}
	DeleteHashPos(&It);

	/* Deliver the bounce if there's anything worth mentioning */
	SMTPC_syslog(LOG_DEBUG, "num_bounces = %d\n", num_bounces);

	if (num_bounces == 0) {
		FreeStrBuf(&Msg);
		return;
	}

	if ((StrLength(MyQItem->SenderRoom) == 0) && MyQItem->HaveRelay) {
		const char *RelayUrlStr = "[not found]";
		/* one message that relaying is broken is enough; no extra room error message. */
		StrBuf *RelayDetails = NewStrBuf();

		if (Relay != NULL)
			RelayUrlStr = ChrPtr(Relay->URL);

		StrBufPrintf(RelayDetails,
			     "Relaying via %s failed permanently. \n Reason:\n%s\n Revalidate your relay configuration.",
			     RelayUrlStr,
			     ChrPtr(Msg));
                CtdlAideMessage(ChrPtr(RelayDetails), "Relaying Failed");
		FreeStrBuf(&RelayDetails);
	}

	boundary = NewStrBufPlain(HKEY("=_Citadel_Multipart_"));
	StrBufAppendPrintf(boundary,
			   "%s_%04x%04x",
			   config.c_fqdn,
			   getpid(),
			   ++seq);

	/* Start building our bounce message; go shopping for memory first. */
	BounceMB = NewStrBufPlain(
		NULL,
		1024 + /* mime stuff.... */
		StrLength(Msg) +  /* the bounce information... */
		StrLength(OMsgTxt)); /* the original message */
	if (BounceMB == NULL) {
		FreeStrBuf(&boundary);
		SMTPCM_syslog(LOG_ERR, "Failed to alloc() bounce message.\n");

		return;
	}

	bmsg = (struct CtdlMessage *) malloc(sizeof(struct CtdlMessage));
	if (bmsg == NULL) {
		FreeStrBuf(&boundary);
		FreeStrBuf(&BounceMB);
		SMTPCM_syslog(LOG_ERR, "Failed to alloc() bounce message.\n");

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
				"A message you sent could not be delivered "
				"to some or all of its recipients\n"
				"due to prolonged unavailability "
				"of its destination(s).\n"
				"Giving up on the following addresses:\n\n"
				), 0);
	else
		StrBufAppendBufPlain(
			BounceMB,
			HKEY(
				"A message you sent could not be delivered "
				"to some or all of its recipients.\n"
				"The following addresses "
				"were undeliverable:\n\n"
				), 0);

	StrBufAppendBuf(BounceMB, Msg, 0);
	FreeStrBuf(&Msg);

	if (StrLength(MyQItem->SenderRoom) > 0)
	{
		StrBufAppendBufPlain(
			BounceMB,
			HKEY("The message was originaly posted in: "), 0);
		StrBufAppendBuf(BounceMB, MyQItem->SenderRoom, 0);
		StrBufAppendBufPlain(
			BounceMB,
			HKEY("\n"), 0);
	}

	/* Attach the original message */
	StrBufAppendBufPlain(BounceMB, HKEY("\r\n--"), 0);
	StrBufAppendBuf(BounceMB, boundary, 0);
	StrBufAppendBufPlain(BounceMB, HKEY("\r\n"), 0);
	StrBufAppendBufPlain(BounceMB,
			     HKEY("Content-type: message/rfc822\r\n"), 0);
	StrBufAppendBufPlain(BounceMB,
			     HKEY("Content-Transfer-Encoding: 7bit\r\n"), 0);
	StrBufAppendBufPlain(BounceMB,
			     HKEY("Content-Disposition: inline\r\n"), 0);
	StrBufAppendBufPlain(BounceMB, HKEY("\r\n"), 0);
	StrBufAppendBuf(BounceMB, OMsgTxt, 0);

	/* Close the multipart MIME scope */
	StrBufAppendBufPlain(BounceMB, HKEY("--"), 0);
	StrBufAppendBuf(BounceMB, boundary, 0);
	StrBufAppendBufPlain(BounceMB, HKEY("--\r\n"), 0);

	bmsg->cm_magic = CTDLMESSAGE_MAGIC;
	bmsg->cm_anon_type = MES_NORMAL;
	bmsg->cm_format_type = FMT_RFC822;

	CM_SetField(bmsg, eOriginalRoom, HKEY(MAILROOM));
	CM_SetField(bmsg, eAuthor, HKEY("Citadel"));
	CM_SetField(bmsg, eNodeName, config.c_nodename, strlen(config.c_nodename));
	CM_SetField(bmsg, eMsgSubject, HKEY("Delivery Status Notification (Failure)"));
	CM_SetAsFieldSB(bmsg, eMesageText, &BounceMB);

	/* First try the user who sent the message */
	if (StrLength(MyQItem->BounceTo) == 0) {
		SMTPCM_syslog(LOG_ERR, "No bounce address specified\n");
	}
	else {
		SMTPC_syslog(LOG_DEBUG, "bounce to user? <%s>\n",
		       ChrPtr(MyQItem->BounceTo));
	}

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
	CM_Free(bmsg);
	SMTPCM_syslog(LOG_DEBUG, "Done processing bounces\n");
}

ParsedURL *LoadRelayUrls(OneQueItem *MyQItem,
			 char *Author,
			 char *Address)
{
	int nRelays = 0;
	ParsedURL *RelayUrls = NULL;
	char mxbuf[SIZ];
	ParsedURL **Url = &MyQItem->URL;

	nRelays = get_hosts(mxbuf, "fallbackhost");
	if (nRelays > 0) {
		StrBuf *All;
		StrBuf *One;
		const char *Pos = NULL;
		All = NewStrBufPlain(mxbuf, -1);
		One = NewStrBufPlain(NULL, StrLength(All) + 1);
		
		while ((Pos != StrBufNOTNULL) &&
		       ((Pos == NULL) ||
			!IsEmptyStr(Pos)))
		{
			StrBufExtract_NextToken(One, All, &Pos, '|');
			if (!ParseURL(Url, One, DefaultMXPort)) {
				SMTPC_syslog(LOG_DEBUG,
					     "Failed to parse: %s\n",
					     ChrPtr(One));
			}
			else {
				(*Url)->IsRelay = 1;
				MyQItem->HaveRelay = 1;
			}
		}
		FreeStrBuf(&All);
		FreeStrBuf(&One);
	}
	nRelays = get_hosts(mxbuf, "smarthost");
	if (nRelays > 0) {
		char *User;
		StrBuf *All;
		StrBuf *One;
		const char *Pos = NULL;
		All = NewStrBufPlain(mxbuf, -1);
		One = NewStrBufPlain(NULL, StrLength(All) + 1);
		
		while ((Pos != StrBufNOTNULL) &&
		       ((Pos == NULL) ||
			!IsEmptyStr(Pos)))
		{
			StrBufExtract_NextToken(One, All, &Pos, '|');
			User = strchr(ChrPtr(One), ' ');
			if (User != NULL) {
				if (!strcmp(User + 1, Author) ||
				    !strcmp(User + 1, Address))
					StrBufCutAt(One, 0, User);
				else {
					MyQItem->HaveRelay = 1;
					continue;
				}
			}
			if (!ParseURL(Url, One, DefaultMXPort)) {
				SMTPC_syslog(LOG_DEBUG,
					     "Failed to parse: %s\n",
					     ChrPtr(One));
			}
			else {
				///if (!Url->IsIP)) // todo dupe me fork ipv6
				(*Url)->IsRelay = 1;
				MyQItem->HaveRelay = 1;
			}
		}
		FreeStrBuf(&All);
		FreeStrBuf(&One);
	}
	return RelayUrls;
}
/*
 * smtp_do_procmsg()
 *
 * Called by smtp_do_queue() to handle an individual message.
 */
void smtp_do_procmsg(long msgnum, void *userdata) {
	time_t now;
	int mynumsessions = num_sessions;
	struct CtdlMessage *msg = NULL;
	char *Author = NULL;
	char *Address = NULL;
	char *instr = NULL;
	StrBuf *PlainQItem;
	OneQueItem *MyQItem;
	char *pch;
	HashPos  *It;
	void *vQE;
	long len;
	const char *Key;
	int HaveBuffers = 0;
	StrBuf *Msg =NULL;

	if (mynumsessions > max_sessions_for_outbound_smtp) {
		SMTPC_syslog(LOG_INFO,
			     "skipping because of num jobs %d > %d max_sessions_for_outbound_smtp",
			     mynumsessions,
			     max_sessions_for_outbound_smtp);
	}

	SMTPC_syslog(LOG_DEBUG, "smtp_do_procmsg(%ld)\n", msgnum);
	///strcpy(envelope_from, "");

	msg = CtdlFetchMessage(msgnum, 1);
	if (msg == NULL) {
		SMTPC_syslog(LOG_ERR, "tried %ld but no such message!\n",
		       msgnum);
		return;
	}

	pch = instr = msg->cm_fields[eMesageText];

	/* Strip out the headers (no not amd any other non-instruction) line */
	while (pch != NULL) {
		pch = strchr(pch, '\n');
		if ((pch != NULL) && (*(pch + 1) == '\n')) {
			instr = pch + 2;
			pch = NULL;
		}
	}
	PlainQItem = NewStrBufPlain(instr, -1);
	CM_Free(msg);
	MyQItem = DeserializeQueueItem(PlainQItem, msgnum);
	FreeStrBuf(&PlainQItem);

	if (MyQItem == NULL) {
		SMTPC_syslog(LOG_ERR,
			     "Msg No %ld: already in progress!\n",
			     msgnum);
		return; /* s.b. else is already processing... */
	}

	/*
	 * Postpone delivery if we've already tried recently.
	 */
	now = time(NULL);
	if ((MyQItem->ReattemptWhen != 0) && 
	    (now < MyQItem->ReattemptWhen) &&
	    (run_queue_now == 0))
	{
		SMTPC_syslog(LOG_DEBUG, 
			     "Retry time not yet reached. %ld seconds left.",
			     MyQItem->ReattemptWhen - now);

		It = GetNewHashPos(MyQItem->MailQEntries, 0);
		pthread_mutex_lock(&ActiveQItemsLock);
		{
			if (GetHashPosFromKey(ActiveQItems,
					      LKEY(MyQItem->MessageID),
					      It))
			{
				DeleteEntryFromHash(ActiveQItems, It);
			}
		}
		pthread_mutex_unlock(&ActiveQItemsLock);
		////FreeQueItem(&MyQItem); TODO: DeleteEntryFromHash frees this?
		DeleteHashPos(&It);
		return;
	}

	/*
	 * Bail out if there's no actual message associated with this
	 */
	if (MyQItem->MessageID < 0L) {
		SMTPCM_syslog(LOG_ERR, "no 'msgid' directive found!\n");
		It = GetNewHashPos(MyQItem->MailQEntries, 0);
		pthread_mutex_lock(&ActiveQItemsLock);
		{
			if (GetHashPosFromKey(ActiveQItems,
					      LKEY(MyQItem->MessageID),
					      It))
			{
				DeleteEntryFromHash(ActiveQItems, It);
			}
		}
		pthread_mutex_unlock(&ActiveQItemsLock);
		DeleteHashPos(&It);
		////FreeQueItem(&MyQItem); TODO: DeleteEntryFromHash frees this?
		return;
	}


	It = GetNewHashPos(MyQItem->MailQEntries, 0);
	while (GetNextHashPos(MyQItem->MailQEntries, It, &len, &Key, &vQE))
	{
		MailQEntry *ThisItem = vQE;
		SMTPC_syslog(LOG_DEBUG, "SMTP Queue: Task: <%s> %d\n",
			     ChrPtr(ThisItem->Recipient),
			     ThisItem->Active);
	}
	DeleteHashPos(&It);

	MyQItem->NotYetShutdownDeliveries = 
		MyQItem->ActiveDeliveries = CountActiveQueueEntries(MyQItem, 1);

	/* failsafe against overload: 
	 * will we exceed the limit set? 
	 */
	if ((MyQItem->ActiveDeliveries + mynumsessions > max_sessions_for_outbound_smtp) && 
	    /* if yes, did we reach more than half of the quota? */
	    ((mynumsessions * 2) > max_sessions_for_outbound_smtp) && 
	    /* if... would we ever fit into half of the quota?? */
	    (((MyQItem->ActiveDeliveries * 2)  < max_sessions_for_outbound_smtp)))
	{
		/* abort delivery for another time. */
		SMTPC_syslog(LOG_INFO,
			     "SMTP Queue: skipping because of num jobs %d + %ld > %d max_sessions_for_outbound_smtp",
			     mynumsessions,
			     MyQItem->ActiveDeliveries,
			     max_sessions_for_outbound_smtp);

		It = GetNewHashPos(MyQItem->MailQEntries, 0);
		pthread_mutex_lock(&ActiveQItemsLock);
		{
			if (GetHashPosFromKey(ActiveQItems,
					      LKEY(MyQItem->MessageID),
					      It))
			{
				DeleteEntryFromHash(ActiveQItems, It);
			}
		}
		pthread_mutex_unlock(&ActiveQItemsLock);

		return;
	}


	if (MyQItem->ActiveDeliveries > 0)
	{
		ParsedURL *RelayUrls = NULL;
		int nActivated = 0;
		int n = MsgCount++;
		int m = MyQItem->ActiveDeliveries;
		int i = 1;

		It = GetNewHashPos(MyQItem->MailQEntries, 0);

		Msg = smtp_load_msg(MyQItem, n, &Author, &Address);
		RelayUrls = LoadRelayUrls(MyQItem, Author, Address);
		if ((RelayUrls == NULL) && MyQItem->HaveRelay) {

			while ((i <= m) &&
			       (GetNextHashPos(MyQItem->MailQEntries,
					       It, &len, &Key, &vQE)))
			{
				int KeepBuffers = (i == m);
				MailQEntry *ThisItem = vQE;
				StrBufPrintf(ThisItem->StatusMessage,
					     "No relay configured matching %s / %s", 
					     (Author != NULL)? Author : "",
					     (Address != NULL)? Address : "");
				ThisItem->Status = 5;

				nActivated++;

				if (i > 1) n = MsgCount++;
				syslog(LOG_INFO,
				       "SMTPC: giving up on <%ld> <%s> %d / %d \n",
				       MyQItem->MessageID,
				       ChrPtr(ThisItem->Recipient),
				       i,
				       m);
				(*((int*) userdata)) ++;
				smtp_try_one_queue_entry(MyQItem,
							 ThisItem,
							 Msg,
							 KeepBuffers,
							 n,
							 RelayUrls);

				if (KeepBuffers) HaveBuffers++;

				i++;
			}
			if (Author != NULL) free (Author);
			if (Address != NULL) free (Address);
			DeleteHashPos(&It);

			return;
		}
		if (Author != NULL) free (Author);
		if (Address != NULL) free (Address);

		while ((i <= m) &&
		       (GetNextHashPos(MyQItem->MailQEntries,
				       It, &len, &Key, &vQE)))
		{
			MailQEntry *ThisItem = vQE;

			if (ThisItem->Active == 1)
			{
				int KeepBuffers = (i == m);

				nActivated++;
				if (nActivated % ndelay_count == 0)
					usleep(delay_msec);

				if (i > 1) n = MsgCount++;
				syslog(LOG_DEBUG,
				       "SMTPC: Trying <%ld> <%s> %d / %d \n",
				       MyQItem->MessageID,
				       ChrPtr(ThisItem->Recipient),
				       i,
				       m);
				(*((int*) userdata)) ++;
				smtp_try_one_queue_entry(MyQItem,
							 ThisItem,
							 Msg,
							 KeepBuffers,
							 n,
							 RelayUrls);

				if (KeepBuffers) HaveBuffers++;

				i++;
			}
		}
		DeleteHashPos(&It);
	}
	else
	{
		It = GetNewHashPos(MyQItem->MailQEntries, 0);
		pthread_mutex_lock(&ActiveQItemsLock);
		{
			if (GetHashPosFromKey(ActiveQItems,
					      LKEY(MyQItem->MessageID),
					      It))
			{
				DeleteEntryFromHash(ActiveQItems, It);
			}
			else
			{
				long len;
				const char* Key;
				void *VData;

				SMTPC_syslog(LOG_WARNING,
					     "unable to find QItem with ID[%ld]",
					     MyQItem->MessageID);
				while (GetNextHashPos(ActiveQItems,
						      It,
						      &len,
						      &Key,
						      &VData))
				{
					SMTPC_syslog(LOG_WARNING,
						     "have: ID[%ld]",
						     ((OneQueItem *)VData)->MessageID);
				}
			}

		}
		pthread_mutex_unlock(&ActiveQItemsLock);
		DeleteHashPos(&It);
		////FreeQueItem(&MyQItem); TODO: DeleteEntryFromHash frees this?

// TODO: bounce & delete?

	}
	if (!HaveBuffers) {
		FreeStrBuf (&Msg);
// TODO : free RelayUrls
	}
}



/*
 * smtp_queue_thread()
 *
 * Run through the queue sending out messages.
 */
void smtp_do_queue(void) {
	int num_processed = 0;
	int num_activated = 0;

	pthread_setspecific(MyConKey, (void *)&smtp_queue_CC);
	SMTPCM_syslog(LOG_INFO, "processing outbound queue");

	if (CtdlGetRoom(&CC->room, SMTP_SPOOLOUT_ROOM) != 0) {
		SMTPC_syslog(LOG_ERR, "Cannot find room <%s>", SMTP_SPOOLOUT_ROOM);
	}
	else {
		num_processed = CtdlForEachMessage(MSGS_ALL,
						   0L,
						   NULL,
						   SPOOLMIME,
						   NULL,
						   smtp_do_procmsg,
						   &num_activated);
	}
	SMTPC_syslog(LOG_INFO,
		     "queue run completed; %d messages processed %d activated",
		     num_processed, num_activated);

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
	CtdlCreateRoom(SMTP_SPOOLOUT_ROOM, 3, "", 0, 1, 0, VIEW_QUEUE);

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
	char *pstr;

	if (!threading)
	{
		pstr = getenv("CITSERVER_n_session_max");
		if ((pstr != NULL) && (*pstr != '\0'))
			max_sessions_for_outbound_smtp = atol(pstr); /* how many sessions might be active till we stop adding more smtp jobs */

		pstr = getenv("CITSERVER_smtp_n_delay_count");
		if ((pstr != NULL) && (*pstr != '\0'))
			ndelay_count = atol(pstr); /* every n queued messages we will sleep... */

		pstr = getenv("CITSERVER_smtp_delay");
		if ((pstr != NULL) && (*pstr != '\0'))
			delay_msec = atol(pstr) * 1000; /* this many seconds. */




		CtdlFillSystemContext(&smtp_queue_CC, "SMTP_Send");
		ActiveQItems = NewHash(1, lFlathash);
		pthread_mutex_init(&ActiveQItemsLock, NULL);

		QItemHandlers = NewHash(0, NULL);

		RegisterQItemHandler(HKEY("msgid"),		QItem_Handle_MsgID);
		RegisterQItemHandler(HKEY("envelope_from"),	QItem_Handle_EnvelopeFrom);
		RegisterQItemHandler(HKEY("retry"),		QItem_Handle_retry);
		RegisterQItemHandler(HKEY("attempted"),		QItem_Handle_Attempted);
		RegisterQItemHandler(HKEY("remote"),		QItem_Handle_Recipient);
		RegisterQItemHandler(HKEY("bounceto"),		QItem_Handle_BounceTo);
		RegisterQItemHandler(HKEY("source_room"),	QItem_Handle_SenderRoom);
		RegisterQItemHandler(HKEY("submitted"),		QItem_Handle_Submitted);

		smtp_init_spoolout();

		CtdlRegisterEVCleanupHook(smtp_evq_cleanup);

		CtdlRegisterProtoHook(cmd_smtp, "SMTP", "SMTP utility commands");
		CtdlRegisterSessionHook(smtp_do_queue, EVT_TIMER, PRIO_SEND + 10);
	}

	/* return our Subversion id for the Log */
	return "smtpeventclient";
}
