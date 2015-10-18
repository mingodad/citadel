#include "webcit.h"
#include "webserver.h"
#include "dav.h"

CtxType CTX_MAILSUM = CTX_NONE;
CtxType CTX_MIME_ATACH = CTX_NONE;

static inline void CheckConvertBufs(struct wcsession *WCC)
{
	if (WCC->ConvertBuf1 == NULL)
		WCC->ConvertBuf1 = NewStrBuf();
	if (WCC->ConvertBuf2 == NULL)
		WCC->ConvertBuf2 = NewStrBuf();
}

/*
 * message index functions
 */


void DestroyMimeParts(wc_mime_attachment *Mime)
{
	FreeStrBuf(&Mime->Name);
	FreeStrBuf(&Mime->FileName);
	FreeStrBuf(&Mime->PartNum);
	FreeStrBuf(&Mime->Disposition);
	FreeStrBuf(&Mime->ContentType);
	FreeStrBuf(&Mime->Charset);
	FreeStrBuf(&Mime->Data);
}

void DestroyMime(void *vMime)
{
	wc_mime_attachment *Mime = (wc_mime_attachment*)vMime;
	DestroyMimeParts(Mime);
	free(Mime);
}

void DestroyMessageSummary(void *vMsg)
{
	message_summary *Msg = (message_summary*) vMsg;

	FreeStrBuf(&Msg->from);
	FreeStrBuf(&Msg->to);
	FreeStrBuf(&Msg->subj);
	FreeStrBuf(&Msg->reply_inreplyto);
	FreeStrBuf(&Msg->reply_references);
	FreeStrBuf(&Msg->cccc);
	FreeStrBuf(&Msg->ReplyTo);
	FreeStrBuf(&Msg->hnod);
	FreeStrBuf(&Msg->AllRcpt);
	FreeStrBuf(&Msg->Room);
	FreeStrBuf(&Msg->Rfca);
	FreeStrBuf(&Msg->EnvTo);
	FreeStrBuf(&Msg->OtherNode);

	DeleteHash(&Msg->Attachments);	/* list of Attachments */
	DeleteHash(&Msg->Submessages);
	DeleteHash(&Msg->AttachLinks);
	DeleteHash(&Msg->AllAttach);
	free(Msg);
}



void RegisterMsgHdr(const char *HeaderName, long HdrNLen, ExamineMsgHeaderFunc evaluator, int type)
{
	headereval *ev;
	ev = (headereval*) malloc(sizeof(headereval));
	ev->evaluator = evaluator;
	ev->Type = type;
	Put(MsgHeaderHandler, HeaderName, HdrNLen, ev, NULL);
}

void RegisterMimeRenderer(const char *HeaderName, long HdrNLen, 
			  RenderMimeFunc MimeRenderer,
			  int InlineRenderable,
			  int Priority)
{
	RenderMimeFuncStruct *f;

	f = (RenderMimeFuncStruct*) malloc(sizeof(RenderMimeFuncStruct));
	f->f = MimeRenderer;
	Put(MimeRenderHandler, HeaderName, HdrNLen, f, NULL);
	if (InlineRenderable)
		RegisterEmbeddableMimeType(HeaderName, HdrNLen, 10000 - Priority);
}

/*----------------------------------------------------------------------------*/

/*
 *  comparator for two longs in descending order.
 */
int longcmp_r(const void *s1, const void *s2) {
	long l1;
	long l2;

	l1 = *(long *)GetSearchPayload(s1);
	l2 = *(long *)GetSearchPayload(s2);

	if (l1 > l2) return(-1);
	if (l1 < l2) return(+1);
	return(0);
}

/*
 *  comparator for longs; descending order.
 */
int qlongcmp_r(const void *s1, const void *s2) {
	long l1 = (long) s1;
	long l2 = (long) s2;

	if (l1 > l2) return(-1);
	if (l1 < l2) return(+1);
	return(0);
}

 
/*
 * comparator for message summary structs by ascending subject.
 */
int summcmp_subj(const void *s1, const void *s2) {
	message_summary *summ1;
	message_summary *summ2;
	
	summ1 = (message_summary *)GetSearchPayload(s1);
	summ2 = (message_summary *)GetSearchPayload(s2);
	return strcasecmp(ChrPtr(summ1->subj), ChrPtr(summ2->subj));
}

/*
 * comparator for message summary structs by descending subject.
 */
int summcmp_rsubj(const void *s1, const void *s2) {
	message_summary *summ1;
	message_summary *summ2;
	
	summ1 = (message_summary *)GetSearchPayload(s1);
	summ2 = (message_summary *)GetSearchPayload(s2);
	return strcasecmp(ChrPtr(summ2->subj), ChrPtr(summ1->subj));
}
/*
 * comparator for message summary structs by descending subject.
 */
int groupchange_subj(const void *s1, const void *s2) {
	message_summary *summ1;
	message_summary *summ2;
	
	summ1 = (message_summary *)s1;
	summ2 = (message_summary *)s2;
	return ChrPtr(summ2->subj)[0] != ChrPtr(summ1->subj)[0];
}

/*
 * comparator for message summary structs by ascending sender.
 */
int summcmp_sender(const void *s1, const void *s2) {
	message_summary *summ1;
	message_summary *summ2;
	
	summ1 = (message_summary *)GetSearchPayload(s1);
	summ2 = (message_summary *)GetSearchPayload(s2);
	return strcasecmp(ChrPtr(summ1->from), ChrPtr(summ2->from));
}

/*
 * comparator for message summary structs by descending sender.
 */
int summcmp_rsender(const void *s1, const void *s2) {
	message_summary *summ1;
	message_summary *summ2;
	
	summ1 = (message_summary *)GetSearchPayload(s1);
	summ2 = (message_summary *)GetSearchPayload(s2);
	return strcasecmp(ChrPtr(summ2->from), ChrPtr(summ1->from));
}
/*
 * comparator for message summary structs by descending sender.
 */
int groupchange_sender(const void *s1, const void *s2) {
	message_summary *summ1;
	message_summary *summ2;
	
	summ1 = (message_summary *)s1;
	summ2 = (message_summary *)s2;
	return strcasecmp(ChrPtr(summ2->from), ChrPtr(summ1->from)) != 0;

}

/*
 * comparator for message summary structs by ascending date.
 */
int summcmp_date(const void *s1, const void *s2) {
	message_summary *summ1;
	message_summary *summ2;
	
	summ1 = (message_summary *)GetSearchPayload(s1);
	summ2 = (message_summary *)GetSearchPayload(s2);

	if (summ1->date < summ2->date) return -1;
	else if (summ1->date > summ2->date) return +1;
	else return 0;
}

/*
 * comparator for message summary structs by descending date.
 */
int summcmp_rdate(const void *s1, const void *s2) {
	message_summary *summ1;
	message_summary *summ2;
	
	summ1 = (message_summary *)GetSearchPayload(s1);
	summ2 = (message_summary *)GetSearchPayload(s2);

	if (summ1->date < summ2->date) return +1;
	else if (summ1->date > summ2->date) return -1;
	else return 0;
}

/*
 * comparator for message summary structs by descending date.
 */
const long DAYSECONDS = 24 * 60 * 60;
int groupchange_date(const void *s1, const void *s2) {
	message_summary *summ1;
	message_summary *summ2;
	
	summ1 = (message_summary *)s1;
	summ2 = (message_summary *)s2;

	return (summ1->date % DAYSECONDS) != (summ2->date %DAYSECONDS);
}


/*----------------------------------------------------------------------------*/
/* Don't wanna know... or? */
void examine_pref(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset) {return;}
void examine_suff(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset) {return;}
void examine_path(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset) {return;}
void examine_content_encoding(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
/* TODO: do we care? */
}

void examine_nhdr(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	Msg->nhdr = 0;
	if (!strncasecmp(ChrPtr(HdrLine), "yes", 8))
		Msg->nhdr = 1;
}
int Conditional_ANONYMOUS_MESSAGE(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX(CTX_MAILSUM);
	return Msg->nhdr != 0;
}

void examine_type(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	Msg->format_type = StrToi(HdrLine);
			
}

void examine_from(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	wcsession *WCC = WC;

	CheckConvertBufs(WCC);
	FreeStrBuf(&Msg->from);
	Msg->from = NewStrBufPlain(NULL, StrLength(HdrLine));
	StrBuf_RFC822_2_Utf8(Msg->from, 
			     HdrLine, 
			     WCC->DefaultCharset, 
			     FoundCharset,
			     WCC->ConvertBuf1,
			     WCC->ConvertBuf2);
}
void tmplput_MAIL_SUMM_FROM(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX(CTX_MAILSUM);
	StrBufAppendTemplate(Target, TP, Msg->from, 0);
}

void examine_subj(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	wcsession *WCC = WC;

	CheckConvertBufs(WCC);
	FreeStrBuf(&Msg->subj);
	Msg->subj = NewStrBufPlain(NULL, StrLength(HdrLine));
	StrBuf_RFC822_2_Utf8(Msg->subj, 
			     HdrLine, 
			     WCC->DefaultCharset, 
			     FoundCharset,
			     WCC->ConvertBuf1,
			     WCC->ConvertBuf2);
}
void tmplput_MAIL_SUMM_SUBJECT(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX(CTX_MAILSUM);

	if (TP->Tokens->nParameters == 4)
	{
		const char *pch;
		long len;
		
		GetTemplateTokenString(Target, TP, 3, &pch, &len);
		if ((len > 0)&&
		    (strstr(ChrPtr(Msg->subj), pch) == NULL))
		{
			GetTemplateTokenString(Target, TP, 2, &pch, &len);
			StrBufAppendBufPlain(Target, pch, len, 0);
		}
	}
	StrBufAppendTemplate(Target, TP, Msg->subj, 0);
}
int Conditional_MAIL_SUMM_SUBJECT(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX(CTX_MAILSUM);


	return StrLength(Msg->subj) > 0;
}


void examine_msgn(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	wcsession *WCC = WC;

	CheckConvertBufs(WCC);
	FreeStrBuf(&Msg->reply_inreplyto);
	Msg->reply_inreplyto = NewStrBufPlain(NULL, StrLength(HdrLine));
	Msg->reply_inreplyto_hash = ThreadIdHash(HdrLine);
	StrBuf_RFC822_2_Utf8(Msg->reply_inreplyto, 
			     HdrLine, 
			     WCC->DefaultCharset,
			     FoundCharset,
			     WCC->ConvertBuf1,
			     WCC->ConvertBuf2);
}
void tmplput_MAIL_SUMM_INREPLYTO(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX(CTX_MAILSUM);
	StrBufAppendTemplate(Target, TP, Msg->reply_inreplyto, 0);
}

int Conditional_MAIL_SUMM_UNREAD(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX(CTX_MAILSUM);
	return (Msg->Flags & MSGFLAG_READ) != 0;
}

void examine_wefw(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	wcsession *WCC = WC;

	CheckConvertBufs(WCC);
	FreeStrBuf(&Msg->reply_references);
	Msg->reply_references = NewStrBufPlain(NULL, StrLength(HdrLine));
	Msg->reply_references_hash = ThreadIdHash(HdrLine);
	StrBuf_RFC822_2_Utf8(Msg->reply_references, 
			     HdrLine, 
			     WCC->DefaultCharset, 
			     FoundCharset,
			     WCC->ConvertBuf1,
			     WCC->ConvertBuf2);
}
void tmplput_MAIL_SUMM_REFIDS(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX(CTX_MAILSUM);
	StrBufAppendTemplate(Target, TP, Msg->reply_references, 0);
}

void examine_replyto(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	wcsession *WCC = WC;

	CheckConvertBufs(WCC);
	FreeStrBuf(&Msg->ReplyTo);
	Msg->ReplyTo = NewStrBufPlain(NULL, StrLength(HdrLine));
	StrBuf_RFC822_2_Utf8(Msg->ReplyTo, 
			     HdrLine, 
			     WCC->DefaultCharset, 
			     FoundCharset,
			     WCC->ConvertBuf1,
			     WCC->ConvertBuf2);
	if (Msg->AllRcpt == NULL)
		Msg->AllRcpt = NewStrBufPlain(NULL, StrLength(HdrLine));
	if (StrLength(Msg->AllRcpt) > 0) {
		StrBufAppendBufPlain(Msg->AllRcpt, HKEY(", "), 0);
	}
	StrBufAppendBuf(Msg->AllRcpt, Msg->ReplyTo, 0);
}
void tmplput_MAIL_SUMM_REPLYTO(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX(CTX_MAILSUM);
	StrBufAppendTemplate(Target, TP, Msg->ReplyTo, 0);
}

void examine_cccc(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	wcsession *WCC = WC;

	CheckConvertBufs(WCC);
	FreeStrBuf(&Msg->cccc);
	Msg->cccc = NewStrBufPlain(NULL, StrLength(HdrLine));
	StrBuf_RFC822_2_Utf8(Msg->cccc, 
			     HdrLine, 
			     WCC->DefaultCharset, 
			     FoundCharset,
			     WCC->ConvertBuf1,
			     WCC->ConvertBuf2);
	if (Msg->AllRcpt == NULL)
		Msg->AllRcpt = NewStrBufPlain(NULL, StrLength(HdrLine));
	if (StrLength(Msg->AllRcpt) > 0) {
		StrBufAppendBufPlain(Msg->AllRcpt, HKEY(", "), 0);
	}
	StrBufAppendBuf(Msg->AllRcpt, Msg->cccc, 0);
}
void tmplput_MAIL_SUMM_CCCC(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX(CTX_MAILSUM);
	StrBufAppendTemplate(Target, TP, Msg->cccc, 0);
}


void examine_room(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	if ((StrLength(HdrLine) > 0) &&
	    (strcasecmp(ChrPtr(HdrLine), ChrPtr(WC->CurRoom.name)))) {
		FreeStrBuf(&Msg->Room);
		Msg->Room = NewStrBufDup(HdrLine);		
	}
}
void tmplput_MAIL_SUMM_ORGROOM(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX(CTX_MAILSUM);
	StrBufAppendTemplate(Target, TP, Msg->Room, 0);
}


void examine_rfca(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	FreeStrBuf(&Msg->Rfca);
	Msg->Rfca = NewStrBufDup(HdrLine);
}
void tmplput_MAIL_SUMM_RFCA(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX(CTX_MAILSUM);
	StrBufAppendTemplate(Target, TP, Msg->Rfca, 0);
}
int Conditional_MAIL_SUMM_RFCA(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX(CTX_MAILSUM);
	return StrLength(Msg->Rfca) > 0;
}
int Conditional_MAIL_SUMM_CCCC(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX(CTX_MAILSUM);
	return StrLength(Msg->cccc) > 0;
}
int Conditional_MAIL_SUMM_REPLYTO(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX(CTX_MAILSUM);
	return StrLength(Msg->ReplyTo) > 0;
}

void examine_node(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	wcsession *WCC = WC;

	if ( (StrLength(HdrLine) > 0) &&
	     ((WC->CurRoom.QRFlags & QR_NETWORK)
	      || ((strcasecmp(ChrPtr(HdrLine), ChrPtr(WCC->serv_info->serv_nodename))
		   && (strcasecmp(ChrPtr(HdrLine), ChrPtr(WCC->serv_info->serv_fqdn))))))) {
		FreeStrBuf(&Msg->OtherNode);
		Msg->OtherNode = NewStrBufDup(HdrLine);
	}
}
void tmplput_MAIL_SUMM_OTHERNODE(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX(CTX_MAILSUM);
	StrBufAppendTemplate(Target, TP, Msg->OtherNode, 0);
}
int Conditional_MAIL_SUMM_OTHERNODE(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX(CTX_MAILSUM);
	return StrLength(Msg->OtherNode) > 0;
}

void examine_nvto(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	wcsession *WCC = WC;

	CheckConvertBufs(WCC);
	FreeStrBuf(&Msg->EnvTo);
	Msg->EnvTo = NewStrBufPlain(NULL, StrLength(HdrLine));
	StrBuf_RFC822_2_Utf8(Msg->EnvTo, 
			     HdrLine, 
			     WCC->DefaultCharset, 
			     FoundCharset,
			     WCC->ConvertBuf1,
			     WCC->ConvertBuf2);
}


void examine_rcpt(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	wcsession *WCC = WC;

	CheckConvertBufs(WCC);
	FreeStrBuf(&Msg->to);
	Msg->to = NewStrBufPlain(NULL, StrLength(HdrLine));
	StrBuf_RFC822_2_Utf8(Msg->to, 
			     HdrLine, 
			     WCC->DefaultCharset, 
			     FoundCharset,
			     WCC->ConvertBuf1,
			     WCC->ConvertBuf2);
	if (Msg->AllRcpt == NULL)
		Msg->AllRcpt = NewStrBufPlain(NULL, StrLength(HdrLine));
	if (StrLength(Msg->AllRcpt) > 0) {
		StrBufAppendBufPlain(Msg->AllRcpt, HKEY(", "), 0);
	}
	StrBufAppendBuf(Msg->AllRcpt, Msg->to, 0);
}
void tmplput_MAIL_SUMM_TO(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX(CTX_MAILSUM);
	StrBufAppendTemplate(Target, TP, Msg->to, 0);
}
int Conditional_MAIL_SUMM_TO(StrBuf *Target, WCTemplputParams *TP) 
{
	message_summary *Msg = (message_summary*) CTX(CTX_MAILSUM);
	return StrLength(Msg->to) != 0;
}
int Conditional_MAIL_SUMM_SUBJ(StrBuf *Target, WCTemplputParams *TP) 
{
	message_summary *Msg = (message_summary*) CTX(CTX_MAILSUM);
	return StrLength(Msg->subj) != 0;
}
void tmplput_MAIL_SUMM_ALLRCPT(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX(CTX_MAILSUM);
	StrBufAppendTemplate(Target, TP, Msg->AllRcpt, 0);
}



void tmplput_SUMM_COUNT(StrBuf *Target, WCTemplputParams *TP)
{
	StrBufAppendPrintf(Target, "%d", GetCount( WC->summ));
}

HashList *iterate_get_mailsumm_All(StrBuf *Target, WCTemplputParams *TP)
{
	return WC->summ;
}
void examine_time(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	Msg->date = StrTol(HdrLine);
}

void tmplput_MAIL_SUMM_DATE_BRIEF(StrBuf *Target, WCTemplputParams *TP)
{
	char datebuf[64];
	message_summary *Msg = (message_summary*) CTX(CTX_MAILSUM);
	webcit_fmt_date(datebuf, 64, Msg->date, DATEFMT_BRIEF);
	StrBufAppendBufPlain(Target, datebuf, -1, 0);
}

void tmplput_MAIL_SUMM_EUID(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX(CTX_MAILSUM);
	StrBufAppendTemplate(Target, TP, Msg->euid, 0);
}

void tmplput_MAIL_SUMM_DATE_FULL(StrBuf *Target, WCTemplputParams *TP)
{
	char datebuf[64];
	message_summary *Msg = (message_summary*) CTX(CTX_MAILSUM);
	webcit_fmt_date(datebuf, 64, Msg->date, DATEFMT_FULL);
	StrBufAppendBufPlain(Target, datebuf, -1, 0);
}
void tmplput_MAIL_SUMM_DATE_NO(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX(CTX_MAILSUM);
	StrBufAppendPrintf(Target, "%ld", Msg->date, 0);
}



void render_MAIL(StrBuf *Target, WCTemplputParams *TP, StrBuf *FoundCharset)
{
	wc_mime_attachment *Mime = (wc_mime_attachment *) CTX(CTX_MIME_ATACH);
	const StrBuf *TemplateMime;

	if (Mime->Data == NULL) 
		Mime->Data = NewStrBufPlain(NULL, Mime->length);
	else 
		FlushStrBuf(Mime->Data);
	read_message(Mime->Data, HKEY("view_submessage"), Mime->msgnum, Mime->PartNum, &TemplateMime);
/*
	if ( (!IsEmptyStr(mime_submessages)) && (!section[0]) ) {
		for (i=0; i<num_tokens(mime_submessages, '|'); ++i) {
			extract_token(buf, mime_submessages, i, '|', sizeof buf);
			/ ** use printable_view to suppress buttons * /
			wc_printf("<blockquote>");
			read_message(Mime->msgnum, 1, ChrPtr(Mime->Section));
			wc_printf("</blockquote>");
		}
	}
*/
}

void render_MIME_VCard(StrBuf *Target, WCTemplputParams *TP, StrBuf *FoundCharset)
{
	wc_mime_attachment *Mime = (wc_mime_attachment *) CTX(CTX_MIME_ATACH);
	wcsession *WCC = WC;
	if (StrLength(Mime->Data) == 0)
		MimeLoadData(Mime);
	if (StrLength(Mime->Data) > 0) {
		StrBuf *Buf;
		Buf = NewStrBuf();
		/** If it's my vCard I can edit it */
		if (	(!strcasecmp(ChrPtr(WCC->CurRoom.name), USERCONFIGROOM))
			|| ((StrLength(WCC->CurRoom.name) > 11) &&
			    (!strcasecmp(&(ChrPtr(WCC->CurRoom.name)[11]), USERCONFIGROOM)))
			|| (WCC->CurRoom.view == VIEW_ADDRESSBOOK)
			) {
			StrBufAppendPrintf(Buf, "<a href=\"edit_vcard?msgnum=%ld?partnum=%s\">",
				Mime->msgnum, ChrPtr(Mime->PartNum));
			StrBufAppendPrintf(Buf, "[%s]</a>", _("edit"));
		}

		/* In all cases, display the full card */
		display_vcard(Buf, Mime, 0, 1, NULL, -1);
		FreeStrBuf(&Mime->Data);
		Mime->Data = Buf;
	}

}

void render_MIME_ICS(StrBuf *Target, WCTemplputParams *TP, StrBuf *FoundCharset)
{
	wc_mime_attachment *Mime = (wc_mime_attachment *) CTX(CTX_MIME_ATACH);
	if (StrLength(Mime->Data) == 0) {
		MimeLoadData(Mime);
	}
	if (StrLength(Mime->Data) > 0) {
		cal_process_attachment(Mime);
	}
}



void examine_mime_part(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	const char *Ptr = NULL;
	wc_mime_attachment *Mime;
	StrBuf *Buf;
	wcsession *WCC = WC;

	CheckConvertBufs(WCC);	
	Mime = (wc_mime_attachment*) malloc(sizeof(wc_mime_attachment));
	memset(Mime, 0, sizeof(wc_mime_attachment));
	Mime->msgnum = Msg->msgnum;
	Buf = NewStrBuf();

	Mime->Name = NewStrBuf();
	StrBufExtract_NextToken(Buf, HdrLine, &Ptr, '|');
	StrBuf_RFC822_2_Utf8(Mime->Name, 
			     Buf, 
			     WCC->DefaultCharset, 
			     FoundCharset,
			     WCC->ConvertBuf1,
			     WCC->ConvertBuf2);
	StrBufTrim(Mime->Name);

	StrBufExtract_NextToken(Buf, HdrLine, &Ptr, '|');
	Mime->FileName = NewStrBuf();
	StrBuf_RFC822_2_Utf8(Mime->FileName, 
			     Buf, 
			     WCC->DefaultCharset, 
			     FoundCharset,
			     WCC->ConvertBuf1,
			     WCC->ConvertBuf2);
	StrBufTrim(Mime->FileName);

	Mime->PartNum = NewStrBuf();
	StrBufExtract_NextToken(Mime->PartNum, HdrLine, &Ptr, '|');
	StrBufTrim(Mime->PartNum);
	if (strchr(ChrPtr(Mime->PartNum), '.') != NULL) 
		Mime->level = 2;
	else
		Mime->level = 1;

	Mime->Disposition = NewStrBuf();
	StrBufExtract_NextToken(Mime->Disposition, HdrLine, &Ptr, '|');

	Mime->ContentType = NewStrBuf();
	StrBufExtract_NextToken(Mime->ContentType, HdrLine, &Ptr, '|');
	StrBufTrim(Mime->ContentType);
	StrBufLowerCase(Mime->ContentType);
	if (!strcmp(ChrPtr(Mime->ContentType), "application/octet-stream")) {
		StrBufPlain(Mime->ContentType, 
			    GuessMimeByFilename(SKEY(Mime->FileName)), -1);
	}

	Mime->length = StrBufExtractNext_int(HdrLine, &Ptr, '|');

	StrBufSkip_NTokenS(HdrLine, &Ptr, '|', 1);  /* cbid?? */

	Mime->Charset = NewStrBuf();
	StrBufExtract_NextToken(Mime->Charset, HdrLine, &Ptr, '|');


	if ( (StrLength(Mime->FileName) == 0) && (StrLength(Mime->Name) > 0) ) {
		StrBufAppendBuf(Mime->FileName, Mime->Name, 0);
	}

	if (StrLength(Msg->PartNum) > 0) {
		StrBuf *tmp;
		StrBufPrintf(Buf, "%s.%s", ChrPtr(Msg->PartNum), ChrPtr(Mime->PartNum));
		tmp = Mime->PartNum;
		Mime->PartNum = Buf;
		Buf = tmp;
	}

	if (Msg->AllAttach == NULL)
		Msg->AllAttach = NewHash(1,NULL);
	Put(Msg->AllAttach, SKEY(Mime->PartNum), Mime, DestroyMime);
	FreeStrBuf(&Buf);
}


void evaluate_mime_part(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX(CTX_MAILSUM);
	wc_mime_attachment *Mime = (wc_mime_attachment *) CTX(CTX_MIME_ATACH);
	void *vMimeRenderer;

	/* just print the root-node */
	if ((Mime->level >= 1) &&
	    GetHash(MimeRenderHandler, SKEY(Mime->ContentType), &vMimeRenderer) &&
	    vMimeRenderer != NULL)
	{
		Mime->Renderer = (RenderMimeFuncStruct*) vMimeRenderer;
		if (Msg->Submessages == NULL)
			Msg->Submessages = NewHash(1,NULL);
		Put(Msg->Submessages, SKEY(Mime->PartNum), Mime, reference_free_handler);
	}
	else if ((Mime->level >= 1) &&
		 (!strcasecmp(ChrPtr(Mime->Disposition), "inline"))
		 && (!strncasecmp(ChrPtr(Mime->ContentType), "image/", 6)) ){
		if (Msg->AttachLinks == NULL)
			Msg->AttachLinks = NewHash(1,NULL);
		Put(Msg->AttachLinks, SKEY(Mime->PartNum), Mime, reference_free_handler);
	}
	else if ((Mime->level >= 1) &&
		 (StrLength(Mime->ContentType) > 0) &&
		  ( (!strcasecmp(ChrPtr(Mime->Disposition), "attachment")) 
		    || (!strcasecmp(ChrPtr(Mime->Disposition), "inline"))
		    || (!strcasecmp(ChrPtr(Mime->Disposition), ""))))
	{		
		if (Msg->AttachLinks == NULL)
			Msg->AttachLinks = NewHash(1,NULL);
		Put(Msg->AttachLinks, SKEY(Mime->PartNum), Mime, reference_free_handler);
		if ((strcasecmp(ChrPtr(Mime->ContentType), "application/octet-stream") == 0) && 
		    (StrLength(Mime->FileName) > 0)) {
			FlushStrBuf(Mime->ContentType);
			StrBufAppendBufPlain(Mime->ContentType,
					     GuessMimeByFilename(SKEY(Mime->FileName)),
					     -1, 0);
		}
	}
}

void tmplput_MAIL_SUMM_NATTACH(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX(CTX_MAILSUM);
	StrBufAppendPrintf(Target, "%ld", GetCount(Msg->Attachments));
}


void examine_hnod(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	wcsession *WCC = WC;

	CheckConvertBufs(WCC);
	FreeStrBuf(&Msg->hnod);
	Msg->hnod = NewStrBufPlain(NULL, StrLength(HdrLine));
	StrBuf_RFC822_2_Utf8(Msg->hnod, 
			     HdrLine, 
			     WCC->DefaultCharset, 
			     FoundCharset,
			     WCC->ConvertBuf1,
			     WCC->ConvertBuf2);
}
void tmplput_MAIL_SUMM_H_NODE(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX(CTX_MAILSUM);
	StrBufAppendTemplate(Target, TP, Msg->hnod, 0);
}
int Conditional_MAIL_SUMM_H_NODE(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX(CTX_MAILSUM);
	return StrLength(Msg->hnod) > 0;
}



void examine_text(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	if (Msg->MsgBody->Data == NULL)
		Msg->MsgBody->Data = NewStrBufPlain(NULL, SIZ);
	else
		FlushStrBuf(Msg->MsgBody->Data);
}

void examine_msg4_partnum(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	Msg->MsgBody->PartNum = NewStrBufDup(HdrLine);
	StrBufTrim(Msg->MsgBody->PartNum);
}

void examine_content_lengh(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	Msg->MsgBody->length = StrTol(HdrLine);
	Msg->MsgBody->size_known = 1;
}

void examine_content_type(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	void *vHdr;
	headereval *Hdr;
	StrBuf *Token;
	StrBuf *Value;
	const char* sem;
	const char *eq;
	int len;
	StrBufTrim(HdrLine);
	Msg->MsgBody->ContentType = NewStrBufDup(HdrLine);
	sem = strchr(ChrPtr(HdrLine), ';');

	if (sem != NULL) {
		Token = NewStrBufPlain(NULL, StrLength(HdrLine));
		Value = NewStrBufPlain(NULL, StrLength(HdrLine));
		len = sem - ChrPtr(HdrLine);
		StrBufCutAt(Msg->MsgBody->ContentType, len, NULL);
		while (sem != NULL) {
			while (isspace(*(sem + 1)))
				sem ++;
			StrBufCutLeft(HdrLine, sem - ChrPtr(HdrLine));
			sem = strchr(ChrPtr(HdrLine), ';');
			if (sem != NULL)
				len = sem - ChrPtr(HdrLine);
			else
				len = StrLength(HdrLine);
			FlushStrBuf(Token);
			FlushStrBuf(Value);
			StrBufAppendBufPlain(Token, ChrPtr(HdrLine), len, 0);
			eq = strchr(ChrPtr(Token), '=');
			if (eq != NULL) {
				len = eq - ChrPtr(Token);
				StrBufAppendBufPlain(Value, eq + 1, StrLength(Token) - len - 1, 0); 
				StrBufCutAt(Token, len, NULL);
				StrBufTrim(Value);
			}
			StrBufTrim(Token);

			if (GetHash(MsgHeaderHandler, SKEY(Token), &vHdr) &&
			    (vHdr != NULL)) {
				Hdr = (headereval*)vHdr;
				Hdr->evaluator(Msg, Value, FoundCharset);
			}
			else syslog(LOG_WARNING, "don't know how to handle content type sub-header[%s]\n", ChrPtr(Token));
		}
		FreeStrBuf(&Token);
		FreeStrBuf(&Value);
	}
}


int ReadOneMessageSummary(message_summary *Msg, StrBuf *FoundCharset, StrBuf *Buf)
{
	void *vHdr;
	headereval *Hdr;
	const char *buf;
	const char *ebuf;
	int nBuf;
	long len;
	
	serv_printf("MSG0 %ld|1", Msg->msgnum);	/* ask for headers only */
	
	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, NULL) != 1) {
		return 0;
	}

	while (len = StrBuf_ServGetln(Buf),
	       (len >= 0) && 
	       ((len != 3)  ||
		strcmp(ChrPtr(Buf), "000")))
	{
		buf = ChrPtr(Buf);
		ebuf = strchr(ChrPtr(Buf), '=');
		nBuf = ebuf - buf;
		
		if (GetHash(MsgHeaderHandler, buf, nBuf, &vHdr) &&
		    (vHdr != NULL)) {
			Hdr = (headereval*)vHdr;
			StrBufCutLeft(Buf, nBuf + 1);
			Hdr->evaluator(Msg, Buf, FoundCharset);
		}
		else syslog(LOG_INFO, "Don't know how to handle Message Headerline [%s]", ChrPtr(Buf));
	}
	return 1;
}

void tmplput_MAIL_SUMM_N(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX(CTX_MAILSUM);
	StrBufAppendPrintf(Target, "%ld", Msg->msgnum);
}


void tmplput_MAIL_SUMM_PERMALINK(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX(CTX_MAILSUM);
	StrBuf *perma_link;
	const StrBuf *View;

	perma_link = NewStrBufPlain(HKEY("/readfwd?go="));
	StrBufUrlescAppend(perma_link, WC->CurRoom.name, NULL);
	View = sbstr("view");
	if (View != NULL) {
		StrBufAppendBufPlain(perma_link, HKEY("?view="), 0);
		StrBufAppendBuf(perma_link, View, 0);
	}
	StrBufAppendBufPlain(perma_link, HKEY("?start_reading_at="), 0);
	StrBufAppendPrintf(perma_link, "%ld#%ld", Msg->msgnum, Msg->msgnum);
	StrBufAppendBuf(Target, perma_link, 0);
	FreeStrBuf(&perma_link);
}


int Conditional_MAIL_MIME_ALL(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX(CTX_MAILSUM);
	return GetCount(Msg->Attachments) > 0;
}

int Conditional_MAIL_MIME_SUBMESSAGES(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX(CTX_MAILSUM);
	return GetCount(Msg->Submessages) > 0;
}

int Conditional_MAIL_MIME_ATTACHLINKS(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX(CTX_MAILSUM);
	return GetCount(Msg->AttachLinks) > 0;
}

int Conditional_MAIL_MIME_ATTACH(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX(CTX_MAILSUM);
	return GetCount(Msg->AllAttach) > 0;
}

void tmplput_QUOTED_MAIL_BODY(StrBuf *Target, WCTemplputParams *TP)
{
	const StrBuf *Mime;
        long MsgNum;
	StrBuf *Buf;

	MsgNum = LBstr(TKEY(0));
	Buf = NewStrBuf();
	read_message(Buf, HKEY("view_message_replyquote"), MsgNum, NULL, &Mime);
	StrBufAppendTemplate(Target, TP, Buf, 1);
	FreeStrBuf(&Buf);
}

void tmplput_EDIT_MAIL_BODY(StrBuf *Target, WCTemplputParams *TP)
{
	const StrBuf *Mime;
        long MsgNum;
	StrBuf *Buf;

	MsgNum = LBstr(TKEY(0));
	Buf = NewStrBuf();
	read_message(Buf, HKEY("view_message_edit"), MsgNum, NULL, &Mime);
	StrBufAppendTemplate(Target, TP, Buf, 1);
	FreeStrBuf(&Buf);
}

void tmplput_EDIT_WIKI_BODY(StrBuf *Target, WCTemplputParams *TP)
{
	const StrBuf *Mime;
        long msgnum;
	StrBuf *Buf;

	/* Insert the existing content of the wiki page into the editor.  But we only want
	 * to do this the first time -- if the user is uploading an attachment we don't want
	 * to do it again.
	 */
	if (!havebstr("attach_button")) {
		char *wikipage = strdup(bstr("page"));
		putbstr("format", NewStrBufPlain(HKEY("plain")));
		str_wiki_index(wikipage);
		msgnum = locate_message_by_uid(wikipage);
		free(wikipage);
		if (msgnum >= 0L) {
			Buf = NewStrBuf();
			read_message(Buf, HKEY("view_message_wikiedit"), msgnum, NULL, &Mime);
			StrBufAppendTemplate(Target, TP, Buf, 1);
			FreeStrBuf(&Buf);
		}
	}
}

void tmplput_MAIL_BODY(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX(CTX_MAILSUM);
	StrBufAppendTemplate(Target, TP, Msg->MsgBody->Data, 0);
}


void render_MAIL_variformat(StrBuf *Target, WCTemplputParams *TP, StrBuf *FoundCharset)
{
	/* Messages in legacy Citadel variformat get handled thusly... */
	wc_mime_attachment *Mime = (wc_mime_attachment *) CTX(CTX_MIME_ATACH);
	StrBuf *TTarget = NewStrBufPlain(NULL, StrLength(Mime->Data));
	FmOut(TTarget, "JUSTIFY", Mime->Data);
	FreeStrBuf(&Mime->Data);
	Mime->Data = TTarget;
}

void render_MAIL_text_plain(StrBuf *Target, WCTemplputParams *TP, StrBuf *FoundCharset)
{
	wc_mime_attachment *Mime = (wc_mime_attachment *) CTX(CTX_MIME_ATACH);
	const char *ptr, *pte;
	const char *BufPtr = NULL;
	StrBuf *Line;
	StrBuf *Line1;
	StrBuf *Line2;
	StrBuf *TTarget;
	long Linecount;
	long nEmptyLines;
	int bn = 0;
	int bq = 0;
	int i;
	long len;
#ifdef HAVE_ICONV
	StrBuf *cs = NULL;
	int ConvertIt = 1;
	iconv_t ic = (iconv_t)(-1) ;
#endif

	if ((StrLength(Mime->Data) == 0) && (Mime->length > 0)) {
		FreeStrBuf(&Mime->Data);
		MimeLoadData(Mime);
	}

#ifdef HAVE_ICONV
	if (ConvertIt) {
		if (StrLength(Mime->Charset) != 0)
			cs = Mime->Charset;
		else if (StrLength(FoundCharset) > 0)
			cs = FoundCharset;
		else if (StrLength(WC->DefaultCharset) > 0)
			cs = WC->DefaultCharset;
		if (cs == NULL) {
			ConvertIt = 0;
		}
		else if (!strcasecmp(ChrPtr(cs), "utf-8")) {
			ConvertIt = 0;
		}
		else if (!strcasecmp(ChrPtr(cs), "us-ascii")) {
			ConvertIt = 0;
		}
		else {
			ctdl_iconv_open("UTF-8", ChrPtr(cs), &ic);
			if (ic == (iconv_t)(-1) ) {
				syslog(LOG_WARNING, "%s:%d iconv_open(UTF-8, %s) failed: %s\n",
					__FILE__, __LINE__, ChrPtr(Mime->Charset), strerror(errno));
			}
		}
	}
#endif
	Line = NewStrBufPlain(NULL, SIZ);
	Line1 = NewStrBufPlain(NULL, SIZ);
	Line2 = NewStrBufPlain(NULL, SIZ);
	TTarget = NewStrBufPlain(NULL, StrLength(Mime->Data));
	Linecount = 0;
	nEmptyLines = 0;
	if (StrLength(Mime->Data) > 0) 
		do 
		{
			StrBufSipLine(Line, Mime->Data, &BufPtr);
			bq = 0;
			i = 0;
			ptr = ChrPtr(Line);
			len = StrLength(Line);
			pte = ptr + len;
		
			while ((ptr < pte) &&
			       ((*ptr == '>') ||
				isspace(*ptr)))
			{
				if (*ptr == '>')
					bq++;
				ptr ++;
				i++;
			}
			if (i > 0) StrBufCutLeft(Line, i);
		
			if (StrLength(Line) == 0) {
				if (Linecount == 0)
					continue;
				StrBufAppendBufPlain(TTarget, HKEY("<tt></tt><br>\n"), 0);

				nEmptyLines ++;
				continue;
			}
			nEmptyLines = 0;
			for (i = bn; i < bq; i++)				
				StrBufAppendBufPlain(TTarget, HKEY("<blockquote>"), 0);
			for (i = bq; i < bn; i++)				
				StrBufAppendBufPlain(TTarget, HKEY("</blockquote>"), 0);
#ifdef HAVE_ICONV
			if (ConvertIt) {
				StrBufConvert(Line, Line1, &ic);
			}
#endif
			StrBufAppendBufPlain(TTarget, HKEY("<tt>"), 0);
			UrlizeText(Line1, Line, Line2);

			StrEscAppend(TTarget, Line1, NULL, 0, 0);
			StrBufAppendBufPlain(TTarget, HKEY("</tt><br>\n"), 0);
			bn = bq;
			Linecount ++;
		}
	while ((BufPtr != StrBufNOTNULL) &&
	       (BufPtr != NULL));

	if (nEmptyLines > 0)
		StrBufCutRight(TTarget, nEmptyLines * (sizeof ("<tt></tt><br>\n") - 1));
	for (i = 0; i < bn; i++)				
		StrBufAppendBufPlain(TTarget, HKEY("</blockquote>"), 0);

	StrBufAppendBufPlain(TTarget, HKEY("</i><br>"), 0);
#ifdef HAVE_ICONV
	if (ic != (iconv_t)(-1) ) {
		iconv_close(ic);
	}
#endif

	FreeStrBuf(&Mime->Data);
	Mime->Data = TTarget;
	FlushStrBuf(Mime->ContentType);
	StrBufAppendBufPlain(Mime->ContentType, HKEY("text/html"), 0);
	FlushStrBuf(Mime->Charset);
	StrBufAppendBufPlain(Mime->Charset, HKEY("UTF-8"), 0);
	FreeStrBuf(&Line);
	FreeStrBuf(&Line1);
	FreeStrBuf(&Line2);
}

void render_MAIL_html(StrBuf *Target, WCTemplputParams *TP, StrBuf *FoundCharset)
{
	wc_mime_attachment *Mime = (wc_mime_attachment *) CTX(CTX_MIME_ATACH);
	StrBuf *Buf;

	if (StrLength(Mime->Data) == 0)
		return;

	Buf = NewStrBufPlain(NULL, StrLength(Mime->Data));

	/* HTML is fun, but we've got to strip it first */
	output_html(ChrPtr(Mime->Charset), 
		    (WC->CurRoom.view == VIEW_WIKI ? 1 : 0), 
		    Mime->msgnum,
		    Mime->Data, Buf);
	FreeStrBuf(&Mime->Data);
	Mime->Data = Buf;
}

#ifdef HAVE_MARKDOWN
/*
char * MarkdownHandleURL(const char* SourceURL, const int len, void* something)
{

}
*/
void render_MAIL_markdown(StrBuf *Target, WCTemplputParams *TP, StrBuf *FoundCharset)
{
#include <mkdio.h>
	wc_mime_attachment *Mime = (wc_mime_attachment *) CTX(CTX_MIME_ATACH);
	MMIOT *doc;
	char *md_as_html = NULL;
	const char *format;

	if (StrLength(Mime->Data) == 0)
		return;

	format = bstr("format");

	if ((format == NULL) || 
	    strcmp(format, "plain"))
	{
		doc = mkd_string(ChrPtr(Mime->Data), StrLength(Mime->Data), 0);
		mkd_basename(doc, "/wiki?page=");
		mkd_compile(doc, 0);
		if (mkd_document(doc, &md_as_html) != EOF) {
			FreeStrBuf(&Mime->Data);
			Mime->Data = NewStrBufPlain(md_as_html, -1);
		}
		mkd_cleanup(doc);
	}
}
#endif

void render_MAIL_UNKNOWN(StrBuf *Target, WCTemplputParams *TP, StrBuf *FoundCharset)
{
	wc_mime_attachment *Mime = (wc_mime_attachment *) CTX(CTX_MIME_ATACH);
	/* Unknown weirdness */
	FlushStrBuf(Mime->Data);
	StrBufAppendBufPlain(Mime->Data, _("I don't know how to display "), -1, 0);
	StrBufAppendBuf(Mime->Data, Mime->ContentType, 0);
	StrBufAppendBufPlain(Mime->Data, HKEY("<br>\n"), 0);
}


HashList *iterate_get_mime_All(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX(CTX_MAILSUM);
	return Msg->Attachments;
}
HashList *iterate_get_mime_Submessages(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX(CTX_MAILSUM);
	return Msg->Submessages;
}
HashList *iterate_get_mime_AttachLinks(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX(CTX_MAILSUM);
	return Msg->AttachLinks;
}
HashList *iterate_get_mime_Attachments(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX(CTX_MAILSUM);
	return Msg->AllAttach;
}

void tmplput_MIME_Name(StrBuf *Target, WCTemplputParams *TP)
{
	wc_mime_attachment *mime = (wc_mime_attachment*) CTX(CTX_MIME_ATACH);
	StrBufAppendTemplate(Target, TP, mime->Name, 0);
}

void tmplput_MIME_FileName(StrBuf *Target, WCTemplputParams *TP)
{
	wc_mime_attachment *mime = (wc_mime_attachment*) CTX(CTX_MIME_ATACH);
	StrBufAppendTemplate(Target, TP, mime->FileName, 0);
}

void tmplput_MIME_PartNum(StrBuf *Target, WCTemplputParams *TP)
{
	wc_mime_attachment *mime = (wc_mime_attachment*) CTX(CTX_MIME_ATACH);
	StrBufAppendTemplate(Target, TP, mime->PartNum, 0);
}

void tmplput_MIME_MsgNum(StrBuf *Target, WCTemplputParams *TP)
{
	wc_mime_attachment *mime = (wc_mime_attachment*) CTX(CTX_MIME_ATACH);
	StrBufAppendPrintf(Target, "%ld", mime->msgnum);
}

void tmplput_MIME_Disposition(StrBuf *Target, WCTemplputParams *TP)
{
	wc_mime_attachment *mime = (wc_mime_attachment*) CTX(CTX_MIME_ATACH);
	StrBufAppendTemplate(Target, TP, mime->Disposition, 0);
}

void tmplput_MIME_ContentType(StrBuf *Target, WCTemplputParams *TP)
{
	wc_mime_attachment *mime = (wc_mime_attachment*) CTX(CTX_MIME_ATACH);
	StrBufAppendTemplate(Target, TP, mime->ContentType, 0);
}

void examine_charset(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	Msg->MsgBody->Charset = NewStrBufDup(HdrLine);
}

void tmplput_MIME_Charset(StrBuf *Target, WCTemplputParams *TP)
{
	wc_mime_attachment *mime = (wc_mime_attachment*) CTX(CTX_MIME_ATACH);
	StrBufAppendTemplate(Target, TP, mime->Charset, 0);
}

void tmplput_MIME_Data(StrBuf *Target, WCTemplputParams *TP)
{
	wc_mime_attachment *mime = (wc_mime_attachment*) CTX(CTX_MIME_ATACH);
	if (mime->Renderer != NULL)
		mime->Renderer->f(Target, TP, NULL);
	StrBufAppendTemplate(Target, TP, mime->Data, 0);
	/* TODO: check whether we need to load it now? */
}

void tmplput_MIME_LoadData(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;	
	wc_mime_attachment *mime = (wc_mime_attachment*) CTX(CTX_MIME_ATACH);
	wc_mime_attachment *att;
	
	if (( (!strcasecmp(ChrPtr(mime->Disposition), "inline"))||
	      (!strcasecmp(ChrPtr(mime->Disposition), "attachment"))) && 
	    (strcasecmp(ChrPtr(mime->ContentType), "application/ms-tnef")!=0))
	{
 		
 		int n;
 		char N[64];
 		/* steal this mime part... */
 		att = malloc(sizeof(wc_mime_attachment));
 		memcpy(att, mime, sizeof(wc_mime_attachment));
 		memset(mime, 0, sizeof(wc_mime_attachment));

		if (att->Data == NULL) 
			MimeLoadData(att);

 		if (WCC->attachments == NULL)
 			WCC->attachments = NewHash(1, NULL);
 		/* And add it to the list. */
 		n = snprintf(N, sizeof N, "%d", GetCount(WCC->attachments) + 1);
 		Put(WCC->attachments, N, n, att, DestroyMime);
 	}
}

void tmplput_MIME_Length(StrBuf *Target, WCTemplputParams *TP)
{
	wc_mime_attachment *mime = (wc_mime_attachment*) CTX(CTX_MIME_ATACH);
	StrBufAppendPrintf(Target, "%ld", mime->length);
}

HashList *iterate_get_registered_Attachments(StrBuf *Target, WCTemplputParams *TP)
{
	return WC->attachments;
}

void get_registered_Attachments_Count(StrBuf *Target, WCTemplputParams *TP)
{
	StrBufAppendPrintf(Target, "%ld", GetCount (WC->attachments));
}

void servcmd_do_search(char *buf, long bufsize)
{
	snprintf(buf, bufsize, "MSGS SEARCH|%s", bstr("query"));
}

void servcmd_headers(char *buf, long bufsize)
{
	snprintf(buf, bufsize, "MSGS ALL");
}

void servcmd_readfwd(char *buf, long bufsize)
{
	snprintf(buf, bufsize, "MSGS ALL");
}

void servcmd_readgt(char *buf, long bufsize)
{
	snprintf(buf, bufsize, "MSGS GT|%s", bstr("gt"));
}

void servcmd_readlt(char *buf, long bufsize)
{
	snprintf(buf, bufsize, "MSGS LT|%s", bstr("lt"));
}

void servcmd_readnew(char *buf, long bufsize)
{
	snprintf(buf, bufsize, "MSGS NEW");
}

void servcmd_readold(char *buf, long bufsize)
{
	snprintf(buf, bufsize, "MSGS OLD");
}


/* DO NOT REORDER OR REMOVE ANY OF THESE */
readloop_struct rlid[] = {
	{ {HKEY("do_search")},	servcmd_do_search	},
	{ {HKEY("headers")},	servcmd_headers		},
	{ {HKEY("readfwd")},	servcmd_readfwd		},
	{ {HKEY("readnew")},	servcmd_readnew		},
	{ {HKEY("readold")},	servcmd_readold		},
	{ {HKEY("readgt")},	servcmd_readgt		},
	{ {HKEY("readlt")},	servcmd_readlt		}
};


int ParseMessageListHeaders_Detail(StrBuf *Line, 
				   const char **pos, 
				   message_summary *Msg, 
				   StrBuf *ConversionBuffer)
{
	wcsession *WCC = WC;
	long len;
	long totallen;

	CheckConvertBufs(WCC);

	totallen = StrLength(Line);
	Msg->from = NewStrBufPlain(NULL, totallen);
	len = StrBufExtract_NextToken(ConversionBuffer, Line, pos, '|');
	if (len > 0) {
		/* Handle senders with RFC2047 encoding */
		StrBuf_RFC822_2_Utf8(Msg->from, 
				     ConversionBuffer, 
				     WCC->DefaultCharset, 
				     NULL, 
				     WCC->ConvertBuf1,
				     WCC->ConvertBuf2);
	}
			
	/* node name */
	len = StrBufExtract_NextToken(ConversionBuffer, Line, pos, '|');
	if ((len > 0 ) &&
	    ( ((WCC->CurRoom.QRFlags & QR_NETWORK)
	       || ((strcasecmp(ChrPtr(ConversionBuffer), ChrPtr(WCC->serv_info->serv_nodename))
		    && (strcasecmp(ChrPtr(ConversionBuffer), ChrPtr(WCC->serv_info->serv_fqdn))))))))
	{
		StrBufAppendBufPlain(Msg->from, HKEY(" @ "), 0);
		StrBufAppendBuf(Msg->from, ConversionBuffer, 0);
	}

	/* Internet address (not used)
	 *	StrBufExtract_token(Msg->inetaddr, Line, 4, '|');
	 */
	StrBufSkip_NTokenS(Line, pos, '|', 1);
	Msg->subj = NewStrBufPlain(NULL, totallen);

	FlushStrBuf(ConversionBuffer);
	/* we assume the subject is the last parameter inside of the list; 
	 * thus we don't use the tokenizer to fetch it, since it will hick up 
	 * on tokenizer chars inside of the subjects
	StrBufExtract_NextToken(ConversionBuffer,  Line, pos, '|');
	*/
	len = 0;
	if (*pos != StrBufNOTNULL) {
		len = totallen - (*pos - ChrPtr(Line));
		StrBufPlain(ConversionBuffer, *pos, len);
		*pos = StrBufNOTNULL;
		if ((len > 0) &&
		    (*(ChrPtr(ConversionBuffer) + len - 1) == '|'))
			StrBufCutRight(ConversionBuffer, 1);
	}

	if (len == 0)
		StrBufAppendBufPlain(Msg->subj, _("(no subject)"), -1,0);
	else {
		StrBuf_RFC822_2_Utf8(Msg->subj, 
				     ConversionBuffer, 
				     WCC->DefaultCharset, 
				     NULL,
				     WCC->ConvertBuf1,
				     WCC->ConvertBuf2);
	}

	return 1;
}


int mailview_GetParamsGetServerCall(SharedMessageStatus *Stat, 
				    void **ViewSpecific, 
				    long oper, 
				    char *cmd, 
				    long len,
				    char *filter,
				    long flen)
{
	DoTemplate(HKEY("msg_listview"),NULL,&NoCtx);

	return 200;
}

int mailview_Cleanup(void **ViewSpecific)
{
	/* Note: wDumpContent() will output one additional </div> tag. */
	/* We ought to move this out into template */
	wDumpContent(1);

	return 0;
}


int json_GetParamsGetServerCall(SharedMessageStatus *Stat, 
				void **ViewSpecific, 
				long oper, 
				char *cmd, 
				long len,
				char *filter,
				long flen)
{
	Stat->defaultsortorder = 2;
	Stat->sortit = 1;
	Stat->load_seen = 1;
	/* Generally using maxmsgs|startmsg is not required
	   in mailbox view, but we have a 'safemode' for clients
	   (*cough* Exploder) that simply can't handle too many */
	if (havebstr("maxmsgs"))  Stat->maxmsgs  = ibstr("maxmsgs");
	else                      Stat->maxmsgs  = 9999999;
	if (havebstr("startmsg")) Stat->startmsg = lbstr("startmsg");
	snprintf(cmd, len, "MSGS %s|%s||1",
		 (oper == do_search) ? "SEARCH" : "ALL",
		 (oper == do_search) ? bstr("query") : ""
		);

	return 200;
}
int json_MessageListHdr(SharedMessageStatus *Stat, void **ViewSpecific) 
{
	/* TODO: make a generic function */
	hprintf("HTTP/1.1 200 OK\r\n");
	hprintf("Content-type: application/json; charset=utf-8\r\n");
	hprintf("Server: %s / %s\r\n", PACKAGE_STRING, ChrPtr(WC->serv_info->serv_software));
	hprintf("Connection: close\r\n");
	hprintf("Pragma: no-cache\r\nCache-Control: no-store\r\nExpires:-1\r\n");
	begin_burst();
	return 0;
}

int json_RenderView_or_Tail(SharedMessageStatus *Stat, 
			    void **ViewSpecific, 
			    long oper)
{
	DoTemplate(HKEY("mailsummary_json"),NULL, NULL);
	
	return 0;
}

int json_Cleanup(void **ViewSpecific)
{
	/* Note: wDumpContent() will output one additional </div> tag. */
	/* We ought to move this out into template */
	end_burst();

	return 0;
}



void 
InitModule_MSGRENDERERS
(void)
{
	RegisterCTX(CTX_MAILSUM);
	RegisterCTX(CTX_MIME_ATACH);
	RegisterReadLoopHandlerset(
		VIEW_MAILBOX,
		mailview_GetParamsGetServerCall,
		NULL, /* TODO: is this right? */
		NULL,
		ParseMessageListHeaders_Detail,
		NULL,
		NULL,
		mailview_Cleanup);

	RegisterReadLoopHandlerset(
		VIEW_JSON_LIST,
		json_GetParamsGetServerCall,
		json_MessageListHdr,
		NULL, /* TODO: is this right? */
		ParseMessageListHeaders_Detail,
		NULL,
		json_RenderView_or_Tail,
		json_Cleanup);

	RegisterSortFunc(HKEY("date"), 
			 NULL, 0,
			 summcmp_date,
			 summcmp_rdate,
			 groupchange_date,
			 CTX_MAILSUM);
	RegisterSortFunc(HKEY("subject"), 
			 NULL, 0,
			 summcmp_subj,
			 summcmp_rsubj,
			 groupchange_subj,
			 CTX_MAILSUM);
	RegisterSortFunc(HKEY("sender"),
			 NULL, 0,
			 summcmp_sender,
			 summcmp_rsender,
			 groupchange_sender,
			 CTX_MAILSUM);

	RegisterNamespace("SUMM:COUNT", 0, 0, tmplput_SUMM_COUNT, NULL, CTX_NONE);
	/* iterate over all known mails in WC->summ */
	RegisterIterator("MAIL:SUMM:MSGS", 0, NULL, iterate_get_mailsumm_All,
			 NULL,NULL, CTX_MAILSUM, CTX_NONE, IT_NOFLAG);

	RegisterNamespace("MAIL:SUMM:EUID", 0, 1, tmplput_MAIL_SUMM_EUID, NULL, CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:DATEBRIEF", 0, 0, tmplput_MAIL_SUMM_DATE_BRIEF, NULL, CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:DATEFULL", 0, 0, tmplput_MAIL_SUMM_DATE_FULL, NULL, CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:DATENO",  0, 0, tmplput_MAIL_SUMM_DATE_NO,  NULL, CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:N",       0, 0, tmplput_MAIL_SUMM_N,        NULL, CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:PERMALINK", 0, 0, tmplput_MAIL_SUMM_PERMALINK, NULL, CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:FROM",    0, 2, tmplput_MAIL_SUMM_FROM,     NULL, CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:TO",      0, 2, tmplput_MAIL_SUMM_TO,       NULL, CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:SUBJECT", 0, 4, tmplput_MAIL_SUMM_SUBJECT,  NULL, CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:NTATACH", 0, 0, tmplput_MAIL_SUMM_NATTACH,  NULL, CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:CCCC", 0, 2, tmplput_MAIL_SUMM_CCCC, NULL, CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:REPLYTO", 0, 2, tmplput_MAIL_SUMM_REPLYTO, NULL, CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:H_NODE", 0, 2, tmplput_MAIL_SUMM_H_NODE,  NULL, CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:ALLRCPT", 0, 2, tmplput_MAIL_SUMM_ALLRCPT,  NULL, CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:ORGROOM", 0, 2, tmplput_MAIL_SUMM_ORGROOM,  NULL, CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:RFCA", 0, 2, tmplput_MAIL_SUMM_RFCA, NULL, CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:OTHERNODE", 2, 0, tmplput_MAIL_SUMM_OTHERNODE,  NULL, CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:REFIDS", 0, 1, tmplput_MAIL_SUMM_REFIDS,  NULL, CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:INREPLYTO", 0, 2, tmplput_MAIL_SUMM_INREPLYTO,  NULL, CTX_MAILSUM);
	RegisterNamespace("MAIL:BODY", 0, 2, tmplput_MAIL_BODY,  NULL, CTX_MAILSUM);
	RegisterNamespace("MAIL:QUOTETEXT", 1, 2, tmplput_QUOTED_MAIL_BODY,  NULL, CTX_NONE);
	RegisterNamespace("MAIL:EDITTEXT", 1, 2, tmplput_EDIT_MAIL_BODY,  NULL, CTX_NONE);
	RegisterNamespace("MAIL:EDITWIKI", 1, 2, tmplput_EDIT_WIKI_BODY,  NULL, CTX_NONE);
	RegisterConditional("COND:MAIL:SUMM:RFCA", 0, Conditional_MAIL_SUMM_RFCA,  CTX_MAILSUM);
	RegisterConditional("COND:MAIL:SUMM:CCCC", 0, Conditional_MAIL_SUMM_CCCC,  CTX_MAILSUM);
	RegisterConditional("COND:MAIL:SUMM:REPLYTO", 0, Conditional_MAIL_SUMM_REPLYTO,  CTX_MAILSUM);
	RegisterConditional("COND:MAIL:SUMM:UNREAD", 0, Conditional_MAIL_SUMM_UNREAD, CTX_MAILSUM);
	RegisterConditional("COND:MAIL:SUMM:H_NODE", 0, Conditional_MAIL_SUMM_H_NODE, CTX_MAILSUM);
	RegisterConditional("COND:MAIL:SUMM:OTHERNODE", 0, Conditional_MAIL_SUMM_OTHERNODE, CTX_MAILSUM);
	RegisterConditional("COND:MAIL:SUMM:SUBJECT", 0, Conditional_MAIL_SUMM_SUBJECT, CTX_MAILSUM);
	RegisterConditional("COND:MAIL:ANON", 0, Conditional_ANONYMOUS_MESSAGE, CTX_MAILSUM);
	RegisterConditional("COND:MAIL:TO", 0, Conditional_MAIL_SUMM_TO, CTX_MAILSUM);	
	RegisterConditional("COND:MAIL:SUBJ", 0, Conditional_MAIL_SUMM_SUBJ, CTX_MAILSUM);	

	/* do we have mimetypes to iterate over? */
	RegisterConditional("COND:MAIL:MIME:ATTACH", 0, Conditional_MAIL_MIME_ALL, CTX_MAILSUM);
	RegisterConditional("COND:MAIL:MIME:ATTACH:SUBMESSAGES", 0, Conditional_MAIL_MIME_SUBMESSAGES, CTX_MAILSUM);
	RegisterConditional("COND:MAIL:MIME:ATTACH:LINKS", 0, Conditional_MAIL_MIME_ATTACHLINKS, CTX_MAILSUM);
	RegisterConditional("COND:MAIL:MIME:ATTACH:ATT", 0, Conditional_MAIL_MIME_ATTACH, CTX_MAILSUM);
	RegisterIterator("MAIL:MIME:ATTACH", 0, NULL, iterate_get_mime_All, 
			 NULL, NULL, CTX_MIME_ATACH, CTX_MAILSUM, IT_NOFLAG);
	RegisterIterator("MAIL:MIME:ATTACH:SUBMESSAGES", 0, NULL, iterate_get_mime_Submessages, 
			 NULL, NULL, CTX_MIME_ATACH, CTX_MAILSUM, IT_NOFLAG);
	RegisterIterator("MAIL:MIME:ATTACH:LINKS", 0, NULL, iterate_get_mime_AttachLinks, 
			 NULL, NULL, CTX_MIME_ATACH, CTX_MAILSUM, IT_NOFLAG);
	RegisterIterator("MAIL:MIME:ATTACH:ATT", 0, NULL, iterate_get_mime_Attachments, 
			 NULL, NULL, CTX_MIME_ATACH, CTX_MAILSUM, IT_NOFLAG);

	/* Parts of a mime attachent */
	RegisterNamespace("MAIL:MIME:NAME", 0, 2, tmplput_MIME_Name, NULL, CTX_MIME_ATACH);
	RegisterNamespace("MAIL:MIME:FILENAME", 0, 2, tmplput_MIME_FileName, NULL, CTX_MIME_ATACH);
	RegisterNamespace("MAIL:MIME:PARTNUM", 0, 2, tmplput_MIME_PartNum, NULL, CTX_MIME_ATACH);
	RegisterNamespace("MAIL:MIME:MSGNUM", 0, 2, tmplput_MIME_MsgNum, NULL, CTX_MIME_ATACH);
	RegisterNamespace("MAIL:MIME:DISPOSITION", 0, 2, tmplput_MIME_Disposition, NULL, CTX_MIME_ATACH);
	RegisterNamespace("MAIL:MIME:CONTENTTYPE", 0, 2, tmplput_MIME_ContentType, NULL, CTX_MIME_ATACH);
	RegisterNamespace("MAIL:MIME:CHARSET", 0, 2, tmplput_MIME_Charset, NULL, CTX_MIME_ATACH);
	RegisterNamespace("MAIL:MIME:LENGTH", 0, 2, tmplput_MIME_Length, NULL, CTX_MIME_ATACH);
	RegisterNamespace("MAIL:MIME:DATA", 0, 2, tmplput_MIME_Data, NULL, CTX_MIME_ATACH);
	/* load the actual attachment into WC->attachments; no output!!! */
	RegisterNamespace("MAIL:MIME:LOADDATA", 0, 0, tmplput_MIME_LoadData, NULL, CTX_MIME_ATACH);

	/* iterate the WC->attachments; use the above tokens for their contents */
	RegisterIterator("MSG:ATTACHNAMES", 0, NULL, iterate_get_registered_Attachments, 
			 NULL, NULL, CTX_MIME_ATACH, CTX_NONE, IT_NOFLAG);

	RegisterNamespace("MSG:NATTACH", 0, 0, get_registered_Attachments_Count,  NULL, CTX_NONE);

	/* mime renderers translate an attachment into webcit viewable html text */
	RegisterMimeRenderer(HKEY("message/rfc822"), render_MAIL, 0, 150);
	RegisterMimeRenderer(HKEY("text/x-vcard"), render_MIME_VCard, 1, 201);
	RegisterMimeRenderer(HKEY("text/vcard"), render_MIME_VCard, 1, 200);
//*
	RegisterMimeRenderer(HKEY("text/calendar"), render_MIME_ICS, 1, 501);
	RegisterMimeRenderer(HKEY("application/ics"), render_MIME_ICS, 1, 500);
//*/
	RegisterMimeRenderer(HKEY("text/x-citadel-variformat"), render_MAIL_variformat, 1, 2);
	RegisterMimeRenderer(HKEY("text/plain"), render_MAIL_text_plain, 1, 3);
	RegisterMimeRenderer(HKEY("text"), render_MAIL_text_plain, 1, 1);
	RegisterMimeRenderer(HKEY("text/html"), render_MAIL_html, 1, 100);
#ifdef HAVE_MARKDOWN
	RegisterMimeRenderer(HKEY("text/x-markdown"), render_MAIL_markdown, 1, 30);
#endif
	RegisterMimeRenderer(HKEY(""), render_MAIL_UNKNOWN, 0, 0);

	/* these headers are citserver replies to MSG4 and friends. one evaluator for each */
	RegisterMsgHdr(HKEY("nhdr"), examine_nhdr, 0);
	RegisterMsgHdr(HKEY("type"), examine_type, 0);
	RegisterMsgHdr(HKEY("from"), examine_from, 0);
	RegisterMsgHdr(HKEY("subj"), examine_subj, 0);
	RegisterMsgHdr(HKEY("msgn"), examine_msgn, 0);
	RegisterMsgHdr(HKEY("wefw"), examine_wefw, 0);
	RegisterMsgHdr(HKEY("cccc"), examine_cccc, 0);
	RegisterMsgHdr(HKEY("rep2"), examine_replyto, 0);
	RegisterMsgHdr(HKEY("hnod"), examine_hnod, 0);
	RegisterMsgHdr(HKEY("room"), examine_room, 0);
	RegisterMsgHdr(HKEY("rfca"), examine_rfca, 0);
	RegisterMsgHdr(HKEY("node"), examine_node, 0);
	RegisterMsgHdr(HKEY("rcpt"), examine_rcpt, 0);
	RegisterMsgHdr(HKEY("nvto"), examine_nvto, 0);
	RegisterMsgHdr(HKEY("time"), examine_time, 0);
	RegisterMsgHdr(HKEY("part"), examine_mime_part, 0);
	RegisterMsgHdr(HKEY("text"), examine_text, 1);
	/* these are the content-type headers we get infront of a message; put it into the same hash since it doesn't clash. */
	RegisterMsgHdr(HKEY("X-Citadel-MSG4-Partnum"), examine_msg4_partnum, 0);
	RegisterMsgHdr(HKEY("Content-type"), examine_content_type, 0);
	RegisterMsgHdr(HKEY("Content-length"), examine_content_lengh, 0);
	RegisterMsgHdr(HKEY("Content-transfer-encoding"), examine_content_encoding, 0); /* do we care? */
	RegisterMsgHdr(HKEY("charset"), examine_charset, 0);

	/* Don't care about these... */
	RegisterMsgHdr(HKEY("pref"), examine_pref, 0);
	RegisterMsgHdr(HKEY("suff"), examine_suff, 0);
	RegisterMsgHdr(HKEY("path"), examine_path, 0);
}

void 
InitModule2_MSGRENDERERS
(void)
{
	/* and finalize the anouncement to the server... */
	CreateMimeStr();
}
void 
ServerStartModule_MSGRENDERERS
(void)
{
	MsgHeaderHandler = NewHash(1, NULL);
	MimeRenderHandler = NewHash(1, NULL);
	ReadLoopHandler = NewHash(1, NULL);
}

void 
ServerShutdownModule_MSGRENDERERS
(void)
{
	DeleteHash(&MsgHeaderHandler);
	DeleteHash(&MimeRenderHandler);
	DeleteHash(&ReadLoopHandler);
}



void 
SessionDestroyModule_MSGRENDERERS
(wcsession *sess)
{
	DeleteHash(&sess->attachments);
	FreeStrBuf(&sess->ConvertBuf1);
	FreeStrBuf(&sess->ConvertBuf2);
}
