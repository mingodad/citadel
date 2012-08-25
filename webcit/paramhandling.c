/*
 * parse urlparts and post data
 *
 * Copyright (c) 1996-2012 by the citadel.org team
 *
 * This program is open source software.  You can redistribute it and/or
 * modify it under the terms of the GNU General Public License, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "webcit.h"
#include "webserver.h"

void free_url(void *U)
{
	urlcontent *u = (urlcontent*) U;
	FreeStrBuf(&u->url_data);
	free(u);
}

/*
 * Extract variables from the URL.
 */
void ParseURLParams(StrBuf *url)
{
	const char *aptr, *bptr, *eptr, *up;
	int len, keylen;
	urlcontent *u;
	wcsession *WCC = WC;

	if (WCC->Hdr->urlstrings == NULL)
		WCC->Hdr->urlstrings = NewHash(1, NULL);
	eptr = ChrPtr(url) + StrLength(url);
	up = ChrPtr(url);
	while ((up < eptr) && (!IsEmptyStr(up))) {
		aptr = up;
		while ((aptr < eptr) && (*aptr != '\0') && (*aptr != '='))
			aptr++;
		if (*aptr != '=') {
			return;
		}
		aptr++;
		bptr = aptr;
		while ((bptr < eptr) && (*bptr != '\0')
		      && (*bptr != '&') && (*bptr != '?') && (*bptr != ' ')) {
			bptr++;
		}
		keylen = aptr - up - 1; /* -1 -> '=' */
		if(keylen > sizeof(u->url_key)) {
			syslog(1, "invalid url_key");
			return;
		}

		u = (urlcontent *) malloc(sizeof(urlcontent));
		memcpy(u->url_key, up, keylen);
		u->url_key[keylen] = '\0';
		if (keylen < 0) {
			syslog(1, "invalid url_key");
			free(u);
			return;
		}
		
		if (strncmp(u->url_key, "__", 2) != 0)
		{
			Put(WCC->Hdr->urlstrings, u->url_key, keylen, u, free_url);
			len = bptr - aptr;
			u->url_data = NewStrBufPlain(aptr, len);
			StrBufUnescape(u->url_data, 1);
#ifdef DEBUG_URLSTRINGS
			syslog(9, "%s = [%d]  %s\n", 
				u->url_key, 
				StrLength(u->url_data), 
				ChrPtr(u->url_data)); 
#endif
		}
		else {
			len = bptr - aptr;
			u->url_data = NewStrBufPlain(aptr, len);
			StrBufUnescape(u->url_data, 1);
			syslog(1, "REJECTED because of __ is internal only: %s = [%d]  %s\n", 
				u->url_key, 
				StrLength(u->url_data), 
				ChrPtr(u->url_data)); 
			
			free_url(u);
		}
		up = bptr;
		++up;
	}
}

/*
 * free urlstring memory
 */
void free_urls(void)
{
	DeleteHash(&WC->Hdr->urlstrings);
}

/*
 * Diagnostic function to display the contents of all variables
 */

void dump_vars(void)
{
	wcsession *WCC = WC;
	urlcontent *u;
	void *U;
	long HKLen;
	const char *HKey;
	HashPos *Cursor;
	
	Cursor = GetNewHashPos (WCC->Hdr->urlstrings, 0);
	while (GetNextHashPos(WCC->Hdr->urlstrings, Cursor, &HKLen, &HKey, &U)) {
		u = (urlcontent*) U;
		wc_printf("%38s = %s\n", u->url_key, ChrPtr(u->url_data));
	}
}

/*
 * Return the value of a variable supplied to the current web page (from the url or a form)
 */

const char *XBstr(const char *key, size_t keylen, size_t *len)
{
	void *U;

	if ((WC->Hdr->urlstrings != NULL) && 
	    GetHash(WC->Hdr->urlstrings, key, keylen, &U)) {
		*len = StrLength(((urlcontent *)U)->url_data);
		return ChrPtr(((urlcontent *)U)->url_data);
	}
	else {
		*len = 0;
		return ("");
	}
}

const char *XBSTR(const char *key, size_t *len)
{
	void *U;

	if ((WC->Hdr->urlstrings != NULL) &&
	    GetHash(WC->Hdr->urlstrings, key, strlen (key), &U)){
		*len = StrLength(((urlcontent *)U)->url_data);
		return ChrPtr(((urlcontent *)U)->url_data);
	}
	else {
		*len = 0;
		return ("");
	}
}


const char *BSTR(const char *key)
{
	void *U;

	if ((WC->Hdr->urlstrings != NULL) &&
	    GetHash(WC->Hdr->urlstrings, key, strlen (key), &U))
		return ChrPtr(((urlcontent *)U)->url_data);
	else	
		return ("");
}

const char *Bstr(const char *key, size_t keylen)
{
	void *U;

	if ((WC->Hdr->urlstrings != NULL) && 
	    GetHash(WC->Hdr->urlstrings, key, keylen, &U))
		return ChrPtr(((urlcontent *)U)->url_data);
	else	
		return ("");
}

const StrBuf *SBSTR(const char *key)
{
	void *U;

	if ((WC->Hdr->urlstrings != NULL) &&
	    GetHash(WC->Hdr->urlstrings, key, strlen (key), &U))
		return ((urlcontent *)U)->url_data;
	else	
		return NULL;
}

const StrBuf *SBstr(const char *key, size_t keylen)
{
	void *U;

	if ((WC->Hdr->urlstrings != NULL) && 
	    GetHash(WC->Hdr->urlstrings, key, keylen, &U))
		return ((urlcontent *)U)->url_data;
	else	
		return NULL;
}

long LBstr(const char *key, size_t keylen)
{
	void *U;

	if ((WC->Hdr->urlstrings != NULL) && 
	    GetHash(WC->Hdr->urlstrings, key, keylen, &U))
		return StrTol(((urlcontent *)U)->url_data);
	else	
		return (0);
}

long LBSTR(const char *key)
{
	void *U;

	if ((WC->Hdr->urlstrings != NULL) && 
	    GetHash(WC->Hdr->urlstrings, key, strlen(key), &U))
		return StrTol(((urlcontent *)U)->url_data);
	else	
		return (0);
}

int IBstr(const char *key, size_t keylen)
{
	void *U;

	if ((WC->Hdr->urlstrings != NULL) && 
	    GetHash(WC->Hdr->urlstrings, key, keylen, &U))
		return StrTol(((urlcontent *)U)->url_data);
	else	
		return (0);
}

int IBSTR(const char *key)
{
	void *U;

	if ((WC->Hdr->urlstrings != NULL) && 
	    GetHash(WC->Hdr->urlstrings, key, strlen(key), &U))
		return StrToi(((urlcontent *)U)->url_data);
	else	
		return (0);
}

int HaveBstr(const char *key, size_t keylen)
{
	void *U;

	if ((WC->Hdr->urlstrings != NULL) && 
	    GetHash(WC->Hdr->urlstrings, key, keylen, &U))
		return (StrLength(((urlcontent *)U)->url_data) != 0);
	else	
		return (0);
}

int HAVEBSTR(const char *key)
{
	void *U;

	if ((WC->Hdr->urlstrings != NULL) && 
	    GetHash(WC->Hdr->urlstrings, key, strlen(key), &U))
		return (StrLength(((urlcontent *)U)->url_data) != 0);
	else	
		return (0);
}


int YesBstr(const char *key, size_t keylen)
{
	void *U;

	if ((WC->Hdr->urlstrings != NULL) && 
	    GetHash(WC->Hdr->urlstrings, key, keylen, &U))
		return strcmp( ChrPtr(((urlcontent *)U)->url_data), "yes") == 0;
	else	
		return (0);
}

int YESBSTR(const char *key)
{
	void *U;

	if ((WC->Hdr->urlstrings != NULL) && 
	    GetHash(WC->Hdr->urlstrings, key, strlen(key), &U))
		return strcmp( ChrPtr(((urlcontent *)U)->url_data), "yes") == 0;
	else	
		return (0);
}




/*
 * This function is called by the MIME parser to handle data uploaded by
 * the browser.  Form data, uploaded files, and the data from HTTP PUT
 * operations (such as those found in GroupDAV) all arrive this way.
 *
 * name		Name of the item being uploaded
 * filename	Filename of the item being uploaded
 * partnum	MIME part identifier (not needed)
 * disp		MIME content disposition (not needed)
 * content	The actual data
 * cbtype	MIME content-type
 * cbcharset	Character set
 * length	Content length
 * encoding	MIME encoding type (not needed)
 * cbid		Content ID (not needed)
 * userdata	Not used here
 */
void upload_handler(char *name, char *filename, char *partnum, char *disp,
			void *content, char *cbtype, char *cbcharset,
			size_t length, char *encoding, char *cbid, void *userdata)
{
	wcsession *WCC = WC;
	urlcontent *u;
	long keylen;

#ifdef DEBUG_URLSTRINGS
	syslog(9, "upload_handler() name=%s, type=%s, len=%d", name, cbtype, length);
#endif
	if (WCC->Hdr->urlstrings == NULL)
		WCC->Hdr->urlstrings = NewHash(1, NULL);

	/* Form fields */
	if ( (length > 0) && (IsEmptyStr(cbtype)) ) {
		u = (urlcontent *) malloc(sizeof(urlcontent));
		
		keylen = safestrncpy(u->url_key, name, sizeof(u->url_key));
		u->url_data = NewStrBufPlain(content, length);
		
		if (strncmp(u->url_key, "__", 2) != 0)
		{
			Put(WCC->Hdr->urlstrings, u->url_key, keylen, u, free_url);
		}
		else {
			syslog(1, "REJECTED because of __ is internal only: %s = [%d]  %s\n", 
				u->url_key, 
				StrLength(u->url_data), 
				ChrPtr(u->url_data)); 
			
			free_url(u);
		}
#ifdef DEBUG_URLSTRINGS
		syslog(9, "Key: <%s> len: [%d] Data: <%s>", 
			u->url_key, 
			StrLength(u->url_data), 
			ChrPtr(u->url_data));
#endif
	}

	/* Uploaded files */
	if ( (length > 0) && (!IsEmptyStr(cbtype)) ) {
		WCC->upload = NewStrBufPlain(content, length);
		WCC->upload_length = length;
		WCC->upload_filename = NewStrBufPlain(filename, -1);
		safestrncpy(WCC->upload_content_type, cbtype, sizeof(WC->upload_content_type));
#ifdef DEBUG_URLSTRINGS
		syslog(9, "File: <%s> len: [%ld]", filename, length);
#endif
		
	}

}



void PutBstr(const char *key, long keylen, StrBuf *Value)
{
	urlcontent *u;

	if(keylen > sizeof(u->url_key)) {
		syslog(1, "invalid url_key");
		FreeStrBuf(&Value);
		return;
	}
	u = (urlcontent*)malloc(sizeof(urlcontent));
	memcpy(u->url_key, key, keylen + 1);
	u->url_data = Value;
	Put(WC->Hdr->urlstrings, u->url_key, keylen, u, free_url);
}



int ConditionalBstr(StrBuf *Target, WCTemplputParams *TP)
{
	if(TP->Tokens->nParameters == 3)
		return HaveBstr(TKEY(2));
	else {
		if (IS_NUMBER(TP->Tokens->Params[3]->Type))
		{
			return LBstr(TKEY(2)) == 
				GetTemplateTokenNumber(Target, 
						       TP, 
						       3, 
						       0);
		}
		else {
			const char *pch;
			long len;

			GetTemplateTokenString (Target, TP, 3, &pch, &len);
			return strcmp(Bstr(TKEY(2)), pch) == 0;
		}
	}
}

void tmplput_bstr(StrBuf *Target, WCTemplputParams *TP)
{
	const StrBuf *Buf = SBstr(TKEY(0));
	if (Buf != NULL)
		StrBufAppendTemplate(Target, TP, Buf, 1);
}


void tmplput_bstrforward(StrBuf *Target, WCTemplputParams *TP)
{
	const StrBuf *Buf = SBstr(TKEY(0));
	if (Buf != NULL) {
		StrBufAppendBufPlain(Target, HKEY("?"), 0);		
		StrBufAppendBufPlain(Target, TKEY(0), 0);
		StrBufAppendBufPlain(Target, HKEY("="), 0);		
		StrBufAppendTemplate(Target, TP, Buf, 1);
	}
}

void diagnostics(void)
{
	output_headers(1, 1, 1, 0, 0, 0);
	wc_printf("Session: %d<hr />\n", WC->wc_session);
	wc_printf("Command: <br><PRE>\n");
/*	
StrEscAppend(WC->WBuf, NULL, WC->UrlFragment1, 0, 0);
	wc_printf("<br>\n");
StrEscAppend(WC->WBuf, NULL, WC->UrlFragment12 0, 0);
	wc_printf("<br>\n");
StrEscAppend(WC->WBuf, NULL, WC->UrlFragment3, 0, 0);
*/
	wc_printf("</PRE><hr />\n");
	wc_printf("Variables: <br><PRE>\n");
	dump_vars();
	wc_printf("</PRE><hr />\n");
	wDumpContent(1);
}


void tmplput_url_part(StrBuf *Target, WCTemplputParams *TP)
{
	StrBuf *Name = NULL;
	StrBuf *UrlBuf = NULL;
	wcsession *WCC = WC;
	
	if (WCC != NULL) {
		long n;

		n = GetTemplateTokenNumber(Target, TP, 0, 0);
		if (n == 0) {
			if (WCC->Hdr->HR.Handler != NULL)
				UrlBuf = Name = WCC->Hdr->HR.Handler->Name;
		}
		else if (n == 1) {
			UrlBuf = NewStrBuf();
			StrBufExtract_token(UrlBuf, WCC->Hdr->HR.ReqLine, 0, '/');
		}
		else {
			UrlBuf = NewStrBuf();
			StrBufExtract_token(UrlBuf, WCC->Hdr->HR.ReqLine, 1, '/');
		}

		if (UrlBuf == NULL)  {
			LogTemplateError(Target, "urlbuf", ERR_PARM1, TP, "not set.");
		}
		StrBufAppendTemplate(Target, TP, UrlBuf, 2);
		if (Name == NULL) FreeStrBuf(&UrlBuf);
	}
}


void 
InitModule_PARAMHANDLING
(void)
{
	WebcitAddUrlHandler(HKEY("diagnostics"), "", 0, diagnostics, NEED_URL);

	RegisterConditional(HKEY("COND:BSTR"), 1, ConditionalBstr, CTX_NONE);
	RegisterNamespace("BSTR", 1, 2, tmplput_bstr, NULL, CTX_NONE);
	RegisterNamespace("BSTR:FORWARD", 1, 2, tmplput_bstrforward, NULL, CTX_NONE);
	RegisterNamespace("URLPART", 1, 2, tmplput_url_part, NULL, CTX_NONE);
}


void
SessionAttachModule_PARAMHANDLING
(wcsession *sess)
{
	sess->Hdr->urlstrings = NewHash(1,NULL);
}

void
SessionDetachModule_PARAMHANDLING
(wcsession *sess)
{
	DeleteHash(&sess->Hdr->urlstrings);
	FreeStrBuf(&sess->upload_filename);
}
