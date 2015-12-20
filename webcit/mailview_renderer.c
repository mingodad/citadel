#include "webcit.h"
#include "webserver.h"
#include "dav.h"

static inline void CheckConvertBufs(struct wcsession *WCC)
{
	if (WCC->ConvertBuf1 == NULL)
		WCC->ConvertBuf1 = NewStrBuf();
	if (WCC->ConvertBuf2 == NULL)
		WCC->ConvertBuf2 = NewStrBuf();
}

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

void 
InitModule_MAILVIEW_RENDERERS
(void)
{
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

}
