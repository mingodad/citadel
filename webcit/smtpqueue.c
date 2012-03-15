/* 
 * Display the outbound SMTP queue
 */

#include "webcit.h"
HashList *QItemHandlers = NULL;

/*
 * display one message in the queue
 */
void display_queue_msg(long msgnum)
{
	char buf[1024];
	char keyword[32];
	int in_body = 0;
	int is_delivery_list = 0;
	time_t submitted = 0;
	time_t attempted = 0;
	time_t last_attempt = 0;
	int number_of_attempts = 0;
	char sender[256];
	char recipients[65536];
	int recipients_len = 0;
	char thisrecp[256];
	char thisdsn[256];
	char thismsg[512];
	int thismsg_len;
	long msgid = 0;
	int len;

	strcpy(sender, "");
	strcpy(recipients, "");
	recipients_len = 0;

	serv_printf("MSG2 %ld", msgnum);
	serv_getln(buf, sizeof buf);
	if (buf[0] != '1') return;

	while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {

		if (!IsEmptyStr(buf)) {
			len = strlen(buf);
			if (buf[len - 1] == 13) {
				buf[len - 1] = 0;
			}
		}

		if ( (IsEmptyStr(buf)) && (in_body == 0) ) {
			in_body = 1;
		}

		if ( (!in_body)
		   && (!strncasecmp(buf, "Content-type: application/x-citadel-delivery-list", 49))
		) {
			is_delivery_list = 1;
		}

		if ( (in_body) && (!is_delivery_list) ) {
			while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
				/* Not a delivery list; flush and return quietly. */
			}
			return;
		}

		if ( (in_body) && (is_delivery_list) ) {
			extract_token(keyword, buf, 0, '|', sizeof keyword);

			if (!strcasecmp(keyword, "msgid")) {
				msgid = extract_long(buf, 1);
			}

			if (!strcasecmp(keyword, "submitted")) {
				submitted = extract_long(buf, 1);
			}

			if (!strcasecmp(keyword, "attempted")) {
				attempted = extract_long(buf, 1);
				++number_of_attempts;
				if (attempted > last_attempt) {
					last_attempt = attempted;
				}
			}

			if (!strcasecmp(keyword, "bounceto")) {
				char *atsign;
				extract_token(sender, buf, 1, '|', sizeof sender);

				/* Strip off local hostname if it's our own */
				atsign = strchr(sender, '@');
				if (atsign != NULL) {
					++atsign;
					if (!strcasecmp(atsign, ChrPtr(WC->serv_info->serv_nodename))) {
						--atsign;
						*atsign = 0;
					}
				}
			}

			if (!strcasecmp(keyword, "remote")) {
				thismsg[0] = 0;

				extract_token(thisrecp, buf, 1, '|', sizeof thisrecp);
				extract_token(thisdsn, buf, 3, '|', sizeof thisdsn);

				if (!IsEmptyStr(thisrecp)) {
					stresc(thismsg, sizeof thismsg, thisrecp, 1, 1);
					if (!IsEmptyStr(thisdsn)) {
						strcat(thismsg, "<br>&nbsp;&nbsp;<i>");
						stresc(&thismsg[strlen(thismsg)], sizeof thismsg,
							thisdsn, 1, 1);
						strcat(thismsg, "</i>");
					}
					thismsg_len = strlen(thismsg);

					if ((recipients_len + thismsg_len + 100) < sizeof recipients) {
						if (!IsEmptyStr(recipients)) {
							strcpy(&recipients[recipients_len], "<br>");
							recipients_len += 6;
						}
						strcpy(&recipients[recipients_len], thismsg);
						recipients_len += thismsg_len;
					}
				}

			}

		}

	}

	wc_printf("<tr><td>");
	wc_printf("%ld<br>", msgnum);
	wc_printf(" <a href=\"javascript:DeleteSMTPqueueMsg(%ld,%ld);\">%s</a>", 
		msgnum, msgid, _("(Delete)")
	);

	wc_printf("</td><td>");
	if (submitted > 0) {
		webcit_fmt_date(buf, 1024, submitted, 1);
		wc_printf("%s", buf);
	}
	else {
		wc_printf("&nbsp;");
	}

	wc_printf("</td><td>");
	if (last_attempt > 0) {
		webcit_fmt_date(buf, 1024, last_attempt, 1);
		wc_printf("%s", buf);
	}
	else {
		wc_printf("&nbsp;");
	}

	wc_printf("</td><td>");
	escputs(sender);

	wc_printf("</td><td>");
	wc_printf("%s", recipients);
	wc_printf("</td></tr>\n");

}


typedef struct _mailq_entry {
	StrBuf *Recipient;
	StrBuf *StatusMessage;
	int Status;
	/**<
	 * 0 = No delivery has yet been attempted
	 * 2 = Delivery was successful
	 * 3 = Transient error like connection problem. Try next remote if available.
	 * 4 = A transient error was experienced ... try again later
	 * 5 = Delivery to this address failed permanently.  The error message
	 *     should be placed in the fourth field so that a bounce message may
	 *     be generated.
	 */

	int n;
	int Active;
}MailQEntry;

typedef struct queueitem {
	long MessageID;
	long QueMsgID;
	long Submitted;
	int FailNow;
	HashList *MailQEntries;
/* copy of the currently parsed item in the MailQEntries list;
 * if null add a new one.
 */
	MailQEntry *Current;
	time_t ReattemptWhen;
	time_t Retry;

	long ActiveDeliveries;
	StrBuf *EnvelopeFrom;
	StrBuf *BounceTo;
	StrBuf *SenderRoom;
	ParsedURL *URL;
	ParsedURL *FallBackHost;
} OneQueItem;


typedef void (*QItemHandler)(OneQueItem *Item, StrBuf *Line, const char **Pos);

typedef struct __QItemHandlerStruct {
	QItemHandler H;
} QItemHandlerStruct;

void RegisterQItemHandler(const char *Key, long Len, QItemHandler H)
{
	QItemHandlerStruct *HS = (QItemHandlerStruct*)malloc(sizeof(QItemHandlerStruct));
	HS->H = H;
	Put(QItemHandlers, Key, Len, HS, NULL);
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
	FreeStrBuf(&(*Item)->SenderRoom);
	FreeURL(&(*Item)->URL);
	free(*Item);
	Item = NULL;
}
void HFreeQueItem(void *Item)
{
	FreeQueItem((OneQueItem**)&Item);
}


OneQueItem *DeserializeQueueItem(StrBuf *RawQItem, long QueMsgID)
{
	OneQueItem *Item;
	const char *pLine = NULL;
	StrBuf *Line;
	StrBuf *Token;
	
	Item = (OneQueItem*)malloc(sizeof(OneQueItem));
	memset(Item, 0, sizeof(OneQueItem));
	Item->Retry = 0;
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
/*
	Put(ActiveQItems,
	    LKEY(Item->MessageID),
	    Item,
	    HFreeQueItem);
*/	

	return Item;
}

void tmplput_MailQID(StrBuf *Target, WCTemplputParams *TP)
{
	OneQueItem *Item = (OneQueItem*) CTX;
	StrBufAppendPrintf(Target, "%ld", Item->QueMsgID);;
}
void tmplput_MailQPayloadID(StrBuf *Target, WCTemplputParams *TP)
{
	OneQueItem *Item = (OneQueItem*) CTX;
	StrBufAppendPrintf(Target, "%ld", Item->MessageID);
}
void tmplput_MailQBounceTo(StrBuf *Target, WCTemplputParams *TP)
{
	OneQueItem *Item = (OneQueItem*) CTX;
	StrBufAppendTemplate(Target, TP, Item->BounceTo, 0);
}
void tmplput_MailQAttempted(StrBuf *Target, WCTemplputParams *TP)
{
        char datebuf[64];
	OneQueItem *Item = (OneQueItem*) CTX;
        webcit_fmt_date(datebuf, 64, Item->ReattemptWhen, DATEFMT_BRIEF);
        StrBufAppendBufPlain(Target, datebuf, -1, 0);
}
void tmplput_MailQSubmitted(StrBuf *Target, WCTemplputParams *TP)
{
        char datebuf[64];
	OneQueItem *Item = (OneQueItem*) CTX;
        webcit_fmt_date(datebuf, 64, Item->Submitted, DATEFMT_BRIEF);
        StrBufAppendBufPlain(Target, datebuf, -1, 0);
}
void tmplput_MailQEnvelopeFrom(StrBuf *Target, WCTemplputParams *TP)
{
	OneQueItem *Item = (OneQueItem*) CTX;
	StrBufAppendTemplate(Target, TP, Item->EnvelopeFrom, 0);
}
void tmplput_MailQSourceRoom(StrBuf *Target, WCTemplputParams *TP)
{
	OneQueItem *Item = (OneQueItem*) CTX;
	StrBufAppendTemplate(Target, TP, Item->SenderRoom, 0);
}

int Conditional_MailQ_HaveSourceRoom(StrBuf *Target, WCTemplputParams *TP)
{
	OneQueItem *Item = (OneQueItem*) CTX;
	return StrLength(Item->SenderRoom) > 0;
}

void tmplput_MailQRetry(StrBuf *Target, WCTemplputParams *TP)
{
        char datebuf[64];
	OneQueItem *Item = (OneQueItem*) CTX;

	if (Item->Retry == 0) {
		StrBufAppendBufPlain(Target, _("First Attempt pending"), -1, 0);
	}
	else {
		webcit_fmt_date(datebuf, sizeof(datebuf), Item->Retry, DATEFMT_BRIEF);
		StrBufAppendBufPlain(Target, datebuf, -1, 0);
	}
}

void tmplput_MailQRCPT(StrBuf *Target, WCTemplputParams *TP)
{
	MailQEntry *Entry = (MailQEntry*) CTX;
	StrBufAppendTemplate(Target, TP, Entry->Recipient, 0);
}
void tmplput_MailQRCPTStatus(StrBuf *Target, WCTemplputParams *TP)
{
	MailQEntry *Entry = (MailQEntry*) CTX;
	StrBufAppendPrintf(Target, "%ld", Entry->Status);
}
void tmplput_MailQStatusMsg(StrBuf *Target, WCTemplputParams *TP)
{
	MailQEntry *Entry = (MailQEntry*) CTX;
	StrBufAppendTemplate(Target, TP, Entry->StatusMessage, 0);
}

HashList *iterate_get_Recipients(StrBuf *Target, WCTemplputParams *TP)
{
	OneQueItem *Item = (OneQueItem*) CTX;
	return Item->MailQEntries;
}


void NewMailQEntry(OneQueItem *Item)
{
	Item->Current = (MailQEntry*) malloc(sizeof(MailQEntry));
	memset(Item->Current, 0, sizeof(MailQEntry));

	if (Item->MailQEntries == NULL)
		Item->MailQEntries = NewHash(1, Flathash);
	Item->Current->StatusMessage = NewStrBuf();
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
	Item->Retry = StrBufExtractNext_int(Line, Pos, '|');
}


void QItem_Handle_Submitted(OneQueItem *Item, StrBuf *Line, const char **Pos)
{
	Item->Submitted = atol(*Pos);

}

void QItem_Handle_Attempted(OneQueItem *Item, StrBuf *Line, const char **Pos)
{
	Item->ReattemptWhen = StrBufExtractNext_int(Line, Pos, '|');
}








void render_QUEUE(wc_mime_attachment *Mime, StrBuf *RawData, StrBuf *FoundCharset)
{
	WCTemplputParams SubTP;

	memset(&SubTP, 0, sizeof(WCTemplputParams));
	SubTP.Filter.ContextType = CTX_MAILQITEM;
	SubTP.Context = DeserializeQueueItem(Mime->Data, Mime->msgnum);
	DoTemplate(HKEY("view_mailq_message"),NULL, &SubTP);
	FreeQueItem ((OneQueItem**)&SubTP.Context);
}

void
ServerShutdownModule_SMTP_QUEUE
(void)
{
	DeleteHash(&QItemHandlers);
}
void
ServerStartModule_SMTP_QUEUE
(void)
{
	QItemHandlers = NewHash(0, NULL);
}

int qview_PrintPageHeader(SharedMessageStatus *Stat, void **ViewSpecific)
{
	output_headers(1, 1, 1, 0, 0, 0);
	return 0;
}

int qview_GetParamsGetServerCall(SharedMessageStatus *Stat,
				 void **ViewSpecific,
				 long oper,
				 char *cmd,
				 long len,
				 char *filter,
				 long flen)
{
	if (!WC->is_aide)
	{
		DoTemplate(HKEY("aide_required"), NULL, NULL);
		end_burst();

		return 300;
	}
	else {
		snprintf(cmd, len, "MSGS ALL|0|1");
		snprintf(filter, flen, "SUBJ|QMSG");
		DoTemplate(HKEY("view_mailq_header"), NULL, NULL);
		return 200;
	}
}

/*
 * Display task view
 */
int qview_LoadMsgFromServer(SharedMessageStatus *Stat, 
                            void **ViewSpecific, 
                            message_summary* Msg, 
                            int is_new, 
                            int i)
{
	wcsession *WCC = WC;
	const StrBuf *Mime;

        /* Not (yet?) needed here? calview *c = (calview *) *ViewSpecific; */
	read_message(WCC->WBuf, HKEY("view_mailq_message_bearer"), Msg->msgnum, NULL, &Mime);

        return 0;
}


int qview_RenderView_or_Tail(SharedMessageStatus *Stat, 
			     void **ViewSpecific, 
			     long oper)
{
	wcsession *WCC = WC;
	WCTemplputParams SubTP;

	if (GetCount(WCC->summ) == 0)
		DoTemplate(HKEY("view_mailq_footer_empty"),NULL, &SubTP);
	else
		DoTemplate(HKEY("view_mailq_footer"),NULL, &SubTP);
	
	return 0;
}
int qview_Cleanup(void **ViewSpecific)
{
	wDumpContent(1);
	return 0;
}

void 
InitModule_SMTP_QUEUE
(void)
{

	RegisterQItemHandler(HKEY("msgid"),		QItem_Handle_MsgID);
	RegisterQItemHandler(HKEY("envelope_from"),	QItem_Handle_EnvelopeFrom);
	RegisterQItemHandler(HKEY("retry"),		QItem_Handle_retry);
	RegisterQItemHandler(HKEY("attempted"),		QItem_Handle_Attempted);
	RegisterQItemHandler(HKEY("remote"),		QItem_Handle_Recipient);
	RegisterQItemHandler(HKEY("bounceto"),		QItem_Handle_BounceTo);
	RegisterQItemHandler(HKEY("source_room"),	QItem_Handle_SenderRoom);
	RegisterQItemHandler(HKEY("submitted"),		QItem_Handle_Submitted);
	RegisterMimeRenderer(HKEY("application/x-citadel-delivery-list"), render_QUEUE, 1, 9000);
	RegisterNamespace("MAILQ:ID", 0, 0, tmplput_MailQID, NULL, CTX_MAILQITEM);
	RegisterNamespace("MAILQ:PAYLOAD:ID", 0, 0, tmplput_MailQPayloadID, NULL, CTX_MAILQITEM);
	RegisterNamespace("MAILQ:BOUNCETO", 0, 1, tmplput_MailQBounceTo, NULL, CTX_MAILQITEM);
	RegisterNamespace("MAILQ:ATTEMPTED", 0, 0, tmplput_MailQAttempted, NULL, CTX_MAILQITEM);
	RegisterNamespace("MAILQ:SUBMITTED", 0, 0, tmplput_MailQSubmitted, NULL, CTX_MAILQITEM);
	RegisterNamespace("MAILQ:ENVELOPEFROM", 0, 1, tmplput_MailQEnvelopeFrom, NULL, CTX_MAILQITEM);
	RegisterNamespace("MAILQ:SRCROOM", 0, 1, tmplput_MailQSourceRoom, NULL, CTX_MAILQITEM);
	RegisterConditional(HKEY("COND:MAILQ:HAVESRCROOM"), 0, Conditional_MailQ_HaveSourceRoom,  CTX_MAILQITEM);
	RegisterNamespace("MAILQ:RETRY", 0, 0, tmplput_MailQRetry, NULL, CTX_MAILQITEM);

	RegisterNamespace("MAILQ:RCPT:ADDR", 0, 1, tmplput_MailQRCPT, NULL, CTX_MAILQ_RCPT);
	RegisterNamespace("MAILQ:RCPT:STATUS", 0, 0, tmplput_MailQRCPTStatus, NULL, CTX_MAILQ_RCPT);
	RegisterNamespace("MAILQ:RCPT:STATUSMSG", 0, 1, tmplput_MailQStatusMsg, NULL, CTX_MAILQ_RCPT);

	RegisterIterator("MAILQ:RCPT", 0, NULL, iterate_get_Recipients, 
			 NULL, NULL, CTX_MAILQ_RCPT, CTX_MAILQITEM, IT_NOFLAG);


	RegisterReadLoopHandlerset(
		VIEW_QUEUE,
		qview_GetParamsGetServerCall,
		qview_PrintPageHeader,
		NULL, /* TODO: is this right? */
		NULL,
		qview_LoadMsgFromServer,
		qview_RenderView_or_Tail,
		qview_Cleanup);

}
