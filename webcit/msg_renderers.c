/*----------------------------------------------------------------------------*/
#include "webcit.h"
#include "webserver.h"

/**
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
	FreeStrBuf(&Msg->hnod);
	FreeStrBuf(&Msg->AllRcpt);
	FreeStrBuf(&Msg->Room);
	FreeStrBuf(&Msg->Rfca);
	FreeStrBuf(&Msg->OtherNode);

	FreeStrBuf(&Msg->reply_to);

	DeleteHash(&Msg->Attachments);  /**< list of Accachments */
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

void RegisterMimeRenderer(const char *HeaderName, long HdrNLen, RenderMimeFunc MimeRenderer)
{
	RenderMimeFuncStruct *f;

	f = (RenderMimeFuncStruct*) malloc(sizeof(RenderMimeFuncStruct));
	f->f = MimeRenderer;
	Put(MimeRenderHandler, HeaderName, HdrNLen, f, NULL);
	
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
	message_summary *Msg = (message_summary*) CTX;
	return Msg->nhdr != 0;
}


void examine_type(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	Msg->format_type = StrToi(HdrLine);
			
}

void examine_from(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	FreeStrBuf(&Msg->from);
	Msg->from = NewStrBufPlain(NULL, StrLength(HdrLine));
	StrBuf_RFC822_to_Utf8(Msg->from, HdrLine, WC->DefaultCharset, FoundCharset);
}
void tmplput_MAIL_SUMM_FROM(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	StrBufAppendTemplate(Target, TP, Msg->from, 0);
}



void examine_subj(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	FreeStrBuf(&Msg->subj);
	Msg->subj = NewStrBufPlain(NULL, StrLength(HdrLine));
	StrBuf_RFC822_to_Utf8(Msg->subj, HdrLine, WC->DefaultCharset, FoundCharset);
}
void tmplput_MAIL_SUMM_SUBJECT(StrBuf *Target, WCTemplputParams *TP)
{/*////TODO: Fwd: and RE: filter!!*/

	message_summary *Msg = (message_summary*) CTX;
	StrBufAppendTemplate(Target, TP, Msg->subj, 0);
}


void examine_msgn(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	FreeStrBuf(&Msg->reply_inreplyto);
	Msg->reply_inreplyto = NewStrBufPlain(NULL, StrLength(HdrLine));
	StrBuf_RFC822_to_Utf8(Msg->reply_inreplyto, HdrLine, WC->DefaultCharset, FoundCharset);
}
void tmplput_MAIL_SUMM_INREPLYTO(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	StrBufAppendTemplate(Target, TP, Msg->reply_inreplyto, 0);
}

int Conditional_MAIL_SUMM_UNREAD(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	return Msg->is_new != 0;
}

void examine_wefw(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	FreeStrBuf(&Msg->reply_references);
	Msg->reply_references = NewStrBufPlain(NULL, StrLength(HdrLine));
	StrBuf_RFC822_to_Utf8(Msg->reply_references, HdrLine, WC->DefaultCharset, FoundCharset);
}
void tmplput_MAIL_SUMM_REFIDS(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	StrBufAppendTemplate(Target, TP, Msg->reply_references, 0);
}


void examine_cccc(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	FreeStrBuf(&Msg->cccc);
	Msg->cccc = NewStrBufPlain(NULL, StrLength(HdrLine));
	StrBuf_RFC822_to_Utf8(Msg->cccc, HdrLine, WC->DefaultCharset, FoundCharset);
	if (Msg->AllRcpt == NULL)
		Msg->AllRcpt = NewStrBufPlain(NULL, StrLength(HdrLine));
	if (StrLength(Msg->AllRcpt) > 0) {
		StrBufAppendBufPlain(Msg->AllRcpt, HKEY(", "), 0);
	}
	StrBufAppendBuf(Msg->AllRcpt, Msg->cccc, 0);
}
void tmplput_MAIL_SUMM_CCCC(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	StrBufAppendTemplate(Target, TP, Msg->cccc, 0);
}


void examine_room(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	if ((StrLength(HdrLine) > 0) &&
	    (strcasecmp(ChrPtr(HdrLine), ChrPtr(WC->wc_roomname)))) {
		FreeStrBuf(&Msg->Room);
		Msg->Room = NewStrBufDup(HdrLine);		
	}
}
void tmplput_MAIL_SUMM_ORGROOM(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	StrBufAppendTemplate(Target, TP, Msg->Room, 0);
}


void examine_rfca(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	FreeStrBuf(&Msg->Rfca);
	Msg->Rfca = NewStrBufDup(HdrLine);
}
void tmplput_MAIL_SUMM_RFCA(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	StrBufAppendTemplate(Target, TP, Msg->Rfca, 0);
}
int Conditional_MAIL_SUMM_RFCA(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	return StrLength(Msg->Rfca) > 0;
}

void examine_node(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	if ( (StrLength(HdrLine) > 0) &&
	     ((WC->room_flags & QR_NETWORK)
	      || ((strcasecmp(ChrPtr(HdrLine), ChrPtr(serv_info.serv_nodename))
		   && (strcasecmp(ChrPtr(HdrLine), ChrPtr(serv_info.serv_fqdn))))))) {
		FreeStrBuf(&Msg->OtherNode);
		Msg->OtherNode = NewStrBufDup(HdrLine);
	}
}
void tmplput_MAIL_SUMM_OTHERNODE(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	StrBufAppendTemplate(Target, TP, Msg->OtherNode, 0);
}
int Conditional_MAIL_SUMM_OTHERNODE(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	return StrLength(Msg->OtherNode) > 0;
}


void examine_rcpt(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	FreeStrBuf(&Msg->to);
	Msg->to = NewStrBufPlain(NULL, StrLength(HdrLine));
	StrBuf_RFC822_to_Utf8(Msg->to, HdrLine, WC->DefaultCharset, FoundCharset);
	if (Msg->AllRcpt == NULL)
		Msg->AllRcpt = NewStrBufPlain(NULL, StrLength(HdrLine));
	if (StrLength(Msg->AllRcpt) > 0) {
		StrBufAppendBufPlain(Msg->AllRcpt, HKEY(", "), 0);
	}
	StrBufAppendBuf(Msg->AllRcpt, Msg->to, 0);
}
void tmplput_MAIL_SUMM_TO(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	StrBufAppendTemplate(Target, TP, Msg->to, 0);
}
void tmplput_MAIL_SUMM_ALLRCPT(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	StrBufAppendTemplate(Target, TP, Msg->AllRcpt, 0);
}


HashList *iterate_get_mailsumm_All(StrBuf *Target, WCTemplputParams *TP)
{
	return WC->summ;
}
int Conditional_ROOM_DISPLAY_MSG(StrBuf *Target, WCTemplputParams *TP) {
  	long num_inset;
	wcsubst *subst;
	wcsession *WCC = WC;
  
	if (GetHash(WCC->vars, HKEY("ITERATE:N"), (void *)&subst) && 
	    (subst != NULL)) {
		num_inset = subst->lvalue;
		if ((num_inset >= WC->startmsg) && (WCC->num_displayed <= WCC->maxmsgs)) {
			WCC->num_displayed = WCC->num_displayed+1;
			return 1; /* Pass GO, collect $200 */
		} 
	}
	return 0;
}
int Conditional_MAIL_SUMM_LASTMSG(StrBuf *Target, WCTemplputParams *TP) {
	wcsubst *nsubst;
	long is_last_n = 0;
  
	if (GetHash(WC->vars, HKEY("ITERATE:LASTN"), (void *)&nsubst) &&
	    (nsubst != NULL)) {
		is_last_n = nsubst->lvalue;
/*
	//GetHash(WC->vars, HKEY("ITERATE:N"), (void *)&n_dsubst);
	//num_inset = n_dsubst->lvalue;
	*/
	/* Is the num_displayed higher than maxmsgs? OR last in iterator */
	}
	return ((WC->num_displayed > WC->maxmsgs) || (is_last_n == 1));
}
void examine_time(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	Msg->date = StrTol(HdrLine);
}

void tmplput_MAIL_SUMM_DATE_BRIEF(StrBuf *Target, WCTemplputParams *TP)
{
	char datebuf[64];
	message_summary *Msg = (message_summary*) CTX;
	webcit_fmt_date(datebuf, Msg->date, 1);
	StrBufAppendBufPlain(Target, datebuf, -1, 0);
}

void tmplput_MAIL_SUMM_DATE_FULL(StrBuf *Target, WCTemplputParams *TP)
{
	char datebuf[64];
	message_summary *Msg = (message_summary*) CTX;
	webcit_fmt_date(datebuf, Msg->date, 0);
	StrBufAppendBufPlain(Target, datebuf, -1, 0);
}
void tmplput_MAIL_SUMM_DATE_NO(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	StrBufAppendPrintf(Target, "%ld", Msg->date, 0);
}



void render_MAIL(wc_mime_attachment *Mime, StrBuf *RawData, StrBuf *FoundCharset)
{
	Mime->Data = NewStrBufPlain(NULL, Mime->length);
	read_message(Mime->Data, HKEY("view_submessage"), Mime->msgnum, 0, Mime->PartNum);
/*
	if ( (!IsEmptyStr(mime_submessages)) && (!section[0]) ) {
		for (i=0; i<num_tokens(mime_submessages, '|'); ++i) {
			extract_token(buf, mime_submessages, i, '|', sizeof buf);
			/ ** use printable_view to suppress buttons * /
			wprintf("<blockquote>");
			read_message(Mime->msgnum, 1, ChrPtr(Mime->Section));
			wprintf("</blockquote>");
		}
	}
*/
}

void render_MIME_VCard(wc_mime_attachment *Mime, StrBuf *RawData, StrBuf *FoundCharset)
{
	wcsession *WCC = WC;
	MimeLoadData(Mime);
	if (StrLength(Mime->Data) > 0) {
		StrBuf *Buf;
		Buf = NewStrBuf();
		/** If it's my vCard I can edit it */
		if (	(!strcasecmp(ChrPtr(WCC->wc_roomname), USERCONFIGROOM))
			|| (!strcasecmp(&(ChrPtr(WCC->wc_roomname)[11]), USERCONFIGROOM))
			|| (WC->wc_view == VIEW_ADDRESSBOOK)
			) {
			StrBufAppendPrintf(Buf, "<a href=\"edit_vcard?msgnum=%ld&partnum=%s\">",
				Mime->msgnum, ChrPtr(Mime->PartNum));
			StrBufAppendPrintf(Buf, "[%s]</a>", _("edit"));
		}

		/* In all cases, display the full card */
		display_vcard(Buf, ChrPtr(Mime->Data), 0, 1, NULL, Mime->msgnum);
		FreeStrBuf(&Mime->Data);
		Mime->Data = Buf;
	}

}

void render_MIME_ICS(wc_mime_attachment *Mime, StrBuf *RawData, StrBuf *FoundCharset)
{
	if (StrLength(Mime->Data) == 0) {
		MimeLoadData(Mime);
	}
	if (StrLength(Mime->Data) > 0) {
		cal_process_attachment(Mime);
	}
}



void examine_mime_part(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	wc_mime_attachment *Mime;
	StrBuf *Buf;
	
	Mime = (wc_mime_attachment*) malloc(sizeof(wc_mime_attachment));
	memset(Mime, 0, sizeof(wc_mime_attachment));
	Mime->msgnum = Msg->msgnum;
	Buf = NewStrBuf();

	Mime->Name = NewStrBuf();
	StrBufExtract_token(Buf, HdrLine, 0, '|');
	StrBuf_RFC822_to_Utf8(Mime->Name, Buf, WC->DefaultCharset, FoundCharset);
	StrBufTrim(Mime->Name);

	StrBufExtract_token(Buf, HdrLine, 1, '|');
	Mime->FileName = NewStrBuf();
	StrBuf_RFC822_to_Utf8(Mime->FileName, Buf, WC->DefaultCharset, FoundCharset);
	StrBufTrim(Mime->FileName);

	Mime->PartNum = NewStrBuf();
	StrBufExtract_token(Mime->PartNum, HdrLine, 2, '|');
	StrBufTrim(Mime->PartNum);
	if (strchr(ChrPtr(Mime->PartNum), '.') != NULL)
		Mime->level = 2;
	else
		Mime->level = 1;

	Mime->Disposition = NewStrBuf();
	StrBufExtract_token(Mime->Disposition, HdrLine, 3, '|');

	Mime->ContentType = NewStrBuf();
	StrBufExtract_token(Mime->ContentType, HdrLine, 4, '|');
	StrBufTrim(Mime->ContentType);
	StrBufLowerCase(Mime->ContentType);

	Mime->length = StrBufExtract_int(HdrLine, 5, '|');

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


void evaluate_mime_part(message_summary *Msg, wc_mime_attachment *Mime)
{
	void *vMimeRenderer;

	/* just print the root-node */
	if ((Mime->level == 1) &&
	    GetHash(MimeRenderHandler, SKEY(Mime->ContentType), &vMimeRenderer) &&
	    vMimeRenderer != NULL)
	{
		Mime->Renderer = (RenderMimeFuncStruct*) vMimeRenderer;
		if (Msg->Submessages == NULL)
			Msg->Submessages = NewHash(1,NULL);
		Put(Msg->Submessages, SKEY(Mime->PartNum), Mime, reference_free_handler);
	}
	else if ((Mime->level == 1) &&
		 (!strcasecmp(ChrPtr(Mime->Disposition), "inline"))
		 && (!strncasecmp(ChrPtr(Mime->ContentType), "image/", 6)) ){
		if (Msg->AttachLinks == NULL)
			Msg->AttachLinks = NewHash(1,NULL);
		Put(Msg->AttachLinks, SKEY(Mime->PartNum), Mime, reference_free_handler);
	}
	else if ((Mime->level == 1) &&
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
	message_summary *Msg = (message_summary*) CTX;
	StrBufAppendPrintf(Target, "%ld", GetCount(Msg->Attachments));
}


void examine_hnod(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	FreeStrBuf(&Msg->hnod);
	Msg->hnod = NewStrBufPlain(NULL, StrLength(HdrLine));
	StrBuf_RFC822_to_Utf8(Msg->hnod, HdrLine, WC->DefaultCharset, FoundCharset);
}
void tmplput_MAIL_SUMM_H_NODE(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	StrBufAppendTemplate(Target, TP, Msg->hnod, 0);
}
int Conditional_MAIL_SUMM_H_NODE(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	return StrLength(Msg->hnod) > 0;
}



void examine_text(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	Msg->MsgBody->Data = NewStrBuf();
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
			else lprintf(1, "don't know how to handle content type sub-header[%s]\n", ChrPtr(Token));
		}
		FreeStrBuf(&Token);
		FreeStrBuf(&Value);
	}
}

void tmplput_MAIL_SUMM_N(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	StrBufAppendPrintf(Target, "%ld", Msg->msgnum);
}



int Conditional_MAIL_MIME_ALL(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	return GetCount(Msg->Attachments) > 0;
}

int Conditional_MAIL_MIME_SUBMESSAGES(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	return GetCount(Msg->Submessages) > 0;
}

int Conditional_MAIL_MIME_ATTACHLINKS(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	return GetCount(Msg->AttachLinks) > 0;
}

int Conditional_MAIL_MIME_ATTACH(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	return GetCount(Msg->AllAttach) > 0;
}



/*----------------------------------------------------------------------------*/
void tmplput_QUOTED_MAIL_BODY(StrBuf *Target, WCTemplputParams *TP)
{
        long MsgNum;
	StrBuf *Buf;

	MsgNum = LBstr(TKEY(0));
	Buf = NewStrBuf();
	read_message(Buf, HKEY("view_message_replyquote"), MsgNum, 0, NULL);
	StrBufAppendTemplate(Target, TP, Buf, 1);
	FreeStrBuf(&Buf);
}

void tmplput_MAIL_BODY(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	StrBufAppendTemplate(Target, TP, Msg->MsgBody->Data, 0);
}


void render_MAIL_variformat(wc_mime_attachment *Mime, StrBuf *RawData, StrBuf *FoundCharset)
{
	/* Messages in legacy Citadel variformat get handled thusly... */
	StrBuf *Target = NewStrBufPlain(NULL, StrLength(Mime->Data));
	FmOut(Target, "JUSTIFY", Mime->Data);
	FreeStrBuf(&Mime->Data);
	Mime->Data = Target;
}

void render_MAIL_text_plain(wc_mime_attachment *Mime, StrBuf *RawData, StrBuf *FoundCharset)
{
	StrBuf *cs = NULL;
	const char *ptr, *pte;
	const char *BufPtr = NULL;
	StrBuf *Line;
	StrBuf *Line1;
	StrBuf *Line2;
	StrBuf *Target;

	int ConvertIt = 1;
	int bn = 0;
	int bq = 0;
	int i, n, done = 0;
	long len;
#ifdef HAVE_ICONV
	iconv_t ic = (iconv_t)(-1) ;
#endif

	if ((StrLength(Mime->Data) == 0) && (Mime->length > 0)) {
		FreeStrBuf(&Mime->Data);
		Mime->Data = NewStrBufPlain(NULL, Mime->length);
		if (!read_message(Mime->Data, HKEY("view_submessage"), Mime->msgnum, 0, Mime->PartNum))
			return;
	}

	/* Boring old 80-column fixed format text gets handled this way... */
	if ((strcasecmp(ChrPtr(Mime->Charset), "us-ascii") == 0) &&
	    (strcasecmp(ChrPtr(Mime->Charset), "UTF-8") == 0))
		ConvertIt = 0;

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
		else {
			ctdl_iconv_open("UTF-8", ChrPtr(cs), &ic);
			if (ic == (iconv_t)(-1) ) {
				lprintf(5, "%s:%d iconv_open(UTF-8, %s) failed: %s\n",
					__FILE__, __LINE__, ChrPtr(Mime->Charset), strerror(errno));
			}
		}
	}
#endif
	Line = NewStrBuf();
	Line1 = NewStrBuf();
	Line2 = NewStrBuf();
	Target = NewStrBufPlain(NULL, StrLength(Mime->Data));

	while ((n = StrBufSipLine(Line, Mime->Data, &BufPtr), n >= 0) && !done)
	{
		done = n == 0;
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
		
		if (StrLength(Line) == 0)
			continue;

		for (i = bn; i < bq; i++)				
			StrBufAppendBufPlain(Target, HKEY("<blockquote>"), 0);
		for (i = bq; i < bn; i++)				
			StrBufAppendBufPlain(Target, HKEY("</blockquote>"), 0);

		if (ConvertIt == 1) {
			StrBufConvert(Line, Line1, &ic);
		}

		StrBufAppendBufPlain(Target, HKEY("<tt>"), 0);
		UrlizeText(Line1, Line, Line2);

		StrEscAppend(Target, Line1, NULL, 0, 0);
		StrBufAppendBufPlain(Target, HKEY("</tt><br />\n"), 0);
		bn = bq;
	}

	for (i = 0; i < bn; i++)				
		StrBufAppendBufPlain(Target, HKEY("</blockquote>"), 0);

	StrBufAppendBufPlain(Target, HKEY("</i><br />"), 0);
#ifdef HAVE_ICONV
	if (ic != (iconv_t)(-1) ) {
		iconv_close(ic);
	}
#endif
	FreeStrBuf(&Mime->Data);
	Mime->Data = Target;
	FreeStrBuf(&Line);
	FreeStrBuf(&Line1);
	FreeStrBuf(&Line2);
}

void render_MAIL_html(wc_mime_attachment *Mime, StrBuf *RawData, StrBuf *FoundCharset)
{
	StrBuf *Buf;

	if (StrLength(Mime->Data) == 0)
		return;

	Buf = NewStrBufPlain(NULL, StrLength(Mime->Data));

	/* HTML is fun, but we've got to strip it first */
	output_html(ChrPtr(Mime->Charset), 
		    (WC->wc_view == VIEW_WIKI ? 1 : 0), 
		    StrToi(Mime->PartNum), 
		    Mime->Data, Buf);
	FreeStrBuf(&Mime->Data);
	Mime->Data = Buf;
}

void render_MAIL_UNKNOWN(wc_mime_attachment *Mime, StrBuf *RawData, StrBuf *FoundCharset)
{
	/* Unknown weirdness */
	FlushStrBuf(Mime->Data);
	StrBufAppendBufPlain(Mime->Data, _("I don't know how to display "), -1, 0);
	StrBufAppendBuf(Mime->Data, Mime->ContentType, 0);
	StrBufAppendBufPlain(Mime->Data, HKEY("<br />\n"), 0);
}






HashList *iterate_get_mime_All(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	return Msg->Attachments;
}
HashList *iterate_get_mime_Submessages(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	return Msg->Submessages;
}
HashList *iterate_get_mime_AttachLinks(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	return Msg->AttachLinks;
}
HashList *iterate_get_mime_Attachments(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	return Msg->AllAttach;
}

void tmplput_MIME_Name(StrBuf *Target, WCTemplputParams *TP)
{
	wc_mime_attachment *mime = (wc_mime_attachment*) CTX;
	StrBufAppendTemplate(Target, TP, mime->Name, 0);
}

void tmplput_MIME_FileName(StrBuf *Target, WCTemplputParams *TP)
{
	wc_mime_attachment *mime = (wc_mime_attachment*) CTX;
	StrBufAppendTemplate(Target, TP, mime->FileName, 0);
}

void tmplput_MIME_PartNum(StrBuf *Target, WCTemplputParams *TP)
{
	wc_mime_attachment *mime = (wc_mime_attachment*) CTX;
	StrBufAppendTemplate(Target, TP, mime->PartNum, 0);
}

void tmplput_MIME_MsgNum(StrBuf *Target, WCTemplputParams *TP)
{
	wc_mime_attachment *mime = (wc_mime_attachment*) CTX;
	StrBufAppendPrintf(Target, "%ld", mime->msgnum);
}

void tmplput_MIME_Disposition(StrBuf *Target, WCTemplputParams *TP)
{
	wc_mime_attachment *mime = (wc_mime_attachment*) CTX;
	StrBufAppendTemplate(Target, TP, mime->Disposition, 0);
}

void tmplput_MIME_ContentType(StrBuf *Target, WCTemplputParams *TP)
{
	wc_mime_attachment *mime = (wc_mime_attachment*) CTX;
	StrBufAppendTemplate(Target, TP, mime->ContentType, 0);
}

void examine_charset(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	Msg->MsgBody->Charset = NewStrBufDup(HdrLine);
}

void tmplput_MIME_Charset(StrBuf *Target, WCTemplputParams *TP)
{
	wc_mime_attachment *mime = (wc_mime_attachment*) CTX;
	StrBufAppendTemplate(Target, TP, mime->Charset, 0);
}

void tmplput_MIME_Data(StrBuf *Target, WCTemplputParams *TP)
{
	wc_mime_attachment *mime = (wc_mime_attachment*) CTX;
	if (mime->Renderer != NULL)
		mime->Renderer->f(mime, NULL, NULL);
	StrBufAppendTemplate(Target, TP, mime->Data, 0);
	/* TODO: check whether we need to load it now? */
}

void tmplput_MIME_LoadData(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;	
	wc_mime_attachment *mime = (wc_mime_attachment*) CTX;
	wc_mime_attachment *att;
	
	if ( (!strcasecmp(ChrPtr(mime->Disposition), "inline"))||
	     (!strcasecmp(ChrPtr(mime->Disposition), "attachment")) ) 
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
	wc_mime_attachment *mime = (wc_mime_attachment*) CTX;
	StrBufAppendPrintf(Target, "%ld", mime->length);
}

HashList *iterate_get_registered_Attachments(StrBuf *Target, WCTemplputParams *TP)
{
	return WC->attachments;
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

void servcmd_readnew(char *buf, long bufsize)
{
	snprintf(buf, bufsize, "MSGS NEW");
}

void servcmd_readold(char *buf, long bufsize)
{
	snprintf(buf, bufsize, "MSGS OLD");
}


readloop_struct rlid[] = {
	{ {HKEY("do_search")}, servcmd_do_search},
	{ {HKEY("headers")},   servcmd_headers},
	{ {HKEY("readfwd")},   servcmd_readfwd},
	{ {HKEY("readnew")},   servcmd_readnew},
	{ {HKEY("readold")},   servcmd_readold}
};



void SetAccessCommand(long Oper)
{
	wcsession *WCC = WC;	

	if (WCC->UrlFragment1 != NULL ) {
		FlushStrBuf(WCC->UrlFragment1);
		StrBufAppendBufPlain(WCC->UrlFragment1, 
				     rlid[Oper].name.Key, rlid[Oper].name.len, 0);
	}
	else 
		WCC->UrlFragment1 = NewStrBufPlain(rlid[Oper].name.Key, rlid[Oper].name.len);
}
		




void 
InitModule_MSGRENDERERS
(void)
{
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

	/* iterate over all known mails in WC->summ */
	RegisterIterator("MAIL:SUMM:MSGS", 0, NULL, iterate_get_mailsumm_All,
			 NULL,NULL, CTX_MAILSUM, CTX_NONE, IT_NOFLAG);

	RegisterNamespace("MAIL:SUMM:DATEBRIEF", 0, 0, tmplput_MAIL_SUMM_DATE_BRIEF, CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:DATEFULL", 0, 0, tmplput_MAIL_SUMM_DATE_FULL, CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:DATENO",  0, 0, tmplput_MAIL_SUMM_DATE_NO,  CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:N",       0, 0, tmplput_MAIL_SUMM_N,        CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:FROM",    0, 2, tmplput_MAIL_SUMM_FROM,     CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:TO",      0, 2, tmplput_MAIL_SUMM_TO,       CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:SUBJECT", 0, 4, tmplput_MAIL_SUMM_SUBJECT,  CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:NTATACH", 0, 0, tmplput_MAIL_SUMM_NATTACH,  CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:CCCC", 0, 2, tmplput_MAIL_SUMM_CCCC,  CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:H_NODE", 0, 2, tmplput_MAIL_SUMM_H_NODE,  CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:ALLRCPT", 0, 2, tmplput_MAIL_SUMM_ALLRCPT,  CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:ORGROOM", 0, 2, tmplput_MAIL_SUMM_ORGROOM,  CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:RFCA", 0, 2, tmplput_MAIL_SUMM_RFCA,  CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:OTHERNODE", 2, 0, tmplput_MAIL_SUMM_OTHERNODE,  CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:REFIDS", 0, 0, tmplput_MAIL_SUMM_REFIDS,  CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:INREPLYTO", 0, 2, tmplput_MAIL_SUMM_INREPLYTO,  CTX_MAILSUM);
	RegisterNamespace("MAIL:BODY", 0, 2, tmplput_MAIL_BODY,  CTX_MAILSUM);
	RegisterNamespace("MAIL:QUOTETEXT", 1, 2, tmplput_QUOTED_MAIL_BODY,  CTX_NONE);
	RegisterConditional(HKEY("COND:ROOM:DISPLAYMSG"), 0, Conditional_ROOM_DISPLAY_MSG, CTX_MAILSUM);
	RegisterConditional(HKEY("COND:MAIL:SUMM:LASTMSG"), 0, Conditional_MAIL_SUMM_LASTMSG, CTX_MAILSUM);
	RegisterConditional(HKEY("COND:MAIL:SUMM:RFCA"), 0, Conditional_MAIL_SUMM_RFCA,  CTX_MAILSUM);
	RegisterConditional(HKEY("COND:MAIL:SUMM:UNREAD"), 0, Conditional_MAIL_SUMM_UNREAD, CTX_MAILSUM);
	RegisterConditional(HKEY("COND:MAIL:SUMM:H_NODE"), 0, Conditional_MAIL_SUMM_H_NODE, CTX_MAILSUM);
	RegisterConditional(HKEY("COND:MAIL:SUMM:OTHERNODE"), 0, Conditional_MAIL_SUMM_OTHERNODE, CTX_MAILSUM);
	RegisterConditional(HKEY("COND:MAIL:ANON"), 0, Conditional_ANONYMOUS_MESSAGE, CTX_MAILSUM);


	/* do we have mimetypes to iterate over? */
	RegisterConditional(HKEY("COND:MAIL:MIME:ATTACH"), 0, Conditional_MAIL_MIME_ALL, CTX_MAILSUM);
	RegisterConditional(HKEY("COND:MAIL:MIME:ATTACH:SUBMESSAGES"), 0, Conditional_MAIL_MIME_SUBMESSAGES, CTX_MAILSUM);
	RegisterConditional(HKEY("COND:MAIL:MIME:ATTACH:LINKS"), 0, Conditional_MAIL_MIME_ATTACHLINKS, CTX_MAILSUM);
	RegisterConditional(HKEY("COND:MAIL:MIME:ATTACH:ATT"), 0, Conditional_MAIL_MIME_ATTACH, CTX_MAILSUM);
	RegisterIterator("MAIL:MIME:ATTACH", 0, NULL, iterate_get_mime_All, 
			 NULL, NULL, CTX_MIME_ATACH, CTX_MAILSUM, IT_NOFLAG);
	RegisterIterator("MAIL:MIME:ATTACH:SUBMESSAGES", 0, NULL, iterate_get_mime_Submessages, 
			 NULL, NULL, CTX_MIME_ATACH, CTX_MAILSUM, IT_NOFLAG);
	RegisterIterator("MAIL:MIME:ATTACH:LINKS", 0, NULL, iterate_get_mime_AttachLinks, 
			 NULL, NULL, CTX_MIME_ATACH, CTX_MAILSUM, IT_NOFLAG);
	RegisterIterator("MAIL:MIME:ATTACH:ATT", 0, NULL, iterate_get_mime_Attachments, 
			 NULL, NULL, CTX_MIME_ATACH, CTX_MAILSUM, IT_NOFLAG);

	/* Parts of a mime attachent */
	RegisterNamespace("MAIL:MIME:NAME", 0, 2, tmplput_MIME_Name, CTX_MIME_ATACH);
	RegisterNamespace("MAIL:MIME:FILENAME", 0, 2, tmplput_MIME_FileName, CTX_MIME_ATACH);
	RegisterNamespace("MAIL:MIME:PARTNUM", 0, 2, tmplput_MIME_PartNum, CTX_MIME_ATACH);
	RegisterNamespace("MAIL:MIME:MSGNUM", 0, 2, tmplput_MIME_MsgNum, CTX_MIME_ATACH);
	RegisterNamespace("MAIL:MIME:DISPOSITION", 0, 2, tmplput_MIME_Disposition, CTX_MIME_ATACH);
	RegisterNamespace("MAIL:MIME:CONTENTTYPE", 0, 2, tmplput_MIME_ContentType, CTX_MIME_ATACH);
	RegisterNamespace("MAIL:MIME:CHARSET", 0, 2, tmplput_MIME_Charset, CTX_MIME_ATACH);
	RegisterNamespace("MAIL:MIME:LENGTH", 0, 2, tmplput_MIME_Length, CTX_MIME_ATACH);
	RegisterNamespace("MAIL:MIME:DATA", 0, 2, tmplput_MIME_Data, CTX_MIME_ATACH);
	/* load the actual attachment into WC->attachments; no output!!! */
	RegisterNamespace("MAIL:MIME:LOADDATA", 0, 0, tmplput_MIME_LoadData, CTX_MIME_ATACH);

	/* iterate the WC->attachments; use the above tokens for their contents */
	RegisterIterator("MSG:ATTACHNAMES", 0, NULL, iterate_get_registered_Attachments, 
			 NULL, NULL, CTX_MIME_ATACH, CTX_NONE, IT_NOFLAG);

	/* mime renderers translate an attachment into webcit viewable html text */
	RegisterMimeRenderer(HKEY("message/rfc822"), render_MAIL);
	RegisterMimeRenderer(HKEY("text/x-vcard"), render_MIME_VCard);
	RegisterMimeRenderer(HKEY("text/vcard"), render_MIME_VCard);
	RegisterMimeRenderer(HKEY("text/calendar"), render_MIME_ICS);
	RegisterMimeRenderer(HKEY("application/ics"), render_MIME_ICS);
	RegisterMimeRenderer(HKEY("text/x-citadel-variformat"), render_MAIL_variformat);
	RegisterMimeRenderer(HKEY("text/plain"), render_MAIL_text_plain);
	RegisterMimeRenderer(HKEY("text"), render_MAIL_text_plain);
	RegisterMimeRenderer(HKEY("text/html"), render_MAIL_html);
	RegisterMimeRenderer(HKEY(""), render_MAIL_UNKNOWN);

	/* these headers are citserver replies to MSG4 and friends. one evaluator for each */
	RegisterMsgHdr(HKEY("nhdr"), examine_nhdr, 0);
	RegisterMsgHdr(HKEY("type"), examine_type, 0);
	RegisterMsgHdr(HKEY("from"), examine_from, 0);
	RegisterMsgHdr(HKEY("subj"), examine_subj, 0);
	RegisterMsgHdr(HKEY("msgn"), examine_msgn, 0);
	RegisterMsgHdr(HKEY("wefw"), examine_wefw, 0);
	RegisterMsgHdr(HKEY("cccc"), examine_cccc, 0);
	RegisterMsgHdr(HKEY("hnod"), examine_hnod, 0);
	RegisterMsgHdr(HKEY("room"), examine_room, 0);
	RegisterMsgHdr(HKEY("rfca"), examine_rfca, 0);
	RegisterMsgHdr(HKEY("node"), examine_node, 0);
	RegisterMsgHdr(HKEY("rcpt"), examine_rcpt, 0);
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
