/*
 * $Id: paramhandling.c 6808 2008-12-11 00:00:36Z dothebart $
 *
 * parse urlparts and post data
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

	if (WCC->urlstrings == NULL)
		WCC->urlstrings = NewHash(1, NULL);
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
			lprintf(1, "URLkey to long! [%s]", up);
			continue;
		}

		u = (urlcontent *) malloc(sizeof(urlcontent));
		memcpy(u->url_key, up, keylen);
		u->url_key[keylen] = '\0';
		if (keylen < 0) {
			lprintf(1, "URLkey to long! [%s]", up);
			free(u);
			continue;
		}

		Put(WCC->urlstrings, u->url_key, keylen, u, free_url);
		len = bptr - aptr;
		u->url_data = NewStrBufPlain(aptr, len);
		StrBufUnescape(u->url_data, 1);
	     
		up = bptr;
		++up;
#ifdef DEBUG_URLSTRINGS
		lprintf(9, "%s = [%ld]  %s\n", 
			u->url_key, 
			StrLength(u->url_data), 
			ChrPtr(u->url_data)); 
#endif
	}
}

/*
 * free urlstring memory
 */
void free_urls(void)
{
	DeleteHash(&WC->urlstrings);
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
	
	Cursor = GetNewHashPos (WCC->urlstrings, 0);
	while (GetNextHashPos(WCC->urlstrings, Cursor, &HKLen, &HKey, &U)) {
		u = (urlcontent*) U;
		wprintf("%38s = %s\n", u->url_key, ChrPtr(u->url_data));
	}
}

/*
 * Return the value of a variable supplied to the current web page (from the url or a form)
 */

const char *XBstr(const char *key, size_t keylen, size_t *len)
{
	void *U;

	if ((WC->urlstrings != NULL) && 
	    GetHash(WC->urlstrings, key, keylen, &U)) {
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

	if ((WC->urlstrings != NULL) &&
	    GetHash(WC->urlstrings, key, strlen (key), &U)){
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

	if ((WC->urlstrings != NULL) &&
	    GetHash(WC->urlstrings, key, strlen (key), &U))
		return ChrPtr(((urlcontent *)U)->url_data);
	else	
		return ("");
}

const char *Bstr(const char *key, size_t keylen)
{
	void *U;

	if ((WC->urlstrings != NULL) && 
	    GetHash(WC->urlstrings, key, keylen, &U))
		return ChrPtr(((urlcontent *)U)->url_data);
	else	
		return ("");
}

const StrBuf *SBSTR(const char *key)
{
	void *U;

	if ((WC->urlstrings != NULL) &&
	    GetHash(WC->urlstrings, key, strlen (key), &U))
		return ((urlcontent *)U)->url_data;
	else	
		return NULL;
}

const StrBuf *SBstr(const char *key, size_t keylen)
{
	void *U;

	if ((WC->urlstrings != NULL) && 
	    GetHash(WC->urlstrings, key, keylen, &U))
		return ((urlcontent *)U)->url_data;
	else	
		return NULL;
}

long LBstr(const char *key, size_t keylen)
{
	void *U;

	if ((WC->urlstrings != NULL) && 
	    GetHash(WC->urlstrings, key, keylen, &U))
		return StrTol(((urlcontent *)U)->url_data);
	else	
		return (0);
}

long LBSTR(const char *key)
{
	void *U;

	if ((WC->urlstrings != NULL) && 
	    GetHash(WC->urlstrings, key, strlen(key), &U))
		return StrTol(((urlcontent *)U)->url_data);
	else	
		return (0);
}

int IBstr(const char *key, size_t keylen)
{
	void *U;

	if ((WC->urlstrings != NULL) && 
	    GetHash(WC->urlstrings, key, keylen, &U))
		return StrTol(((urlcontent *)U)->url_data);
	else	
		return (0);
}

int IBSTR(const char *key)
{
	void *U;

	if ((WC->urlstrings != NULL) && 
	    GetHash(WC->urlstrings, key, strlen(key), &U))
		return StrToi(((urlcontent *)U)->url_data);
	else	
		return (0);
}

int HaveBstr(const char *key, size_t keylen)
{
	void *U;

	if ((WC->urlstrings != NULL) && 
	    GetHash(WC->urlstrings, key, keylen, &U))
		return (StrLength(((urlcontent *)U)->url_data) != 0);
	else	
		return (0);
}

int HAVEBSTR(const char *key)
{
	void *U;

	if ((WC->urlstrings != NULL) && 
	    GetHash(WC->urlstrings, key, strlen(key), &U))
		return (StrLength(((urlcontent *)U)->url_data) != 0);
	else	
		return (0);
}


int YesBstr(const char *key, size_t keylen)
{
	void *U;

	if ((WC->urlstrings != NULL) && 
	    GetHash(WC->urlstrings, key, keylen, &U))
		return strcmp( ChrPtr(((urlcontent *)U)->url_data), "yes") == 0;
	else	
		return (0);
}

int YESBSTR(const char *key)
{
	void *U;

	if ((WC->urlstrings != NULL) && 
	    GetHash(WC->urlstrings, key, strlen(key), &U))
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
	urlcontent *u;
#ifdef DEBUG_URLSTRINGS
	lprintf(9, "upload_handler() name=%s, type=%s, len=%d\n", name, cbtype, length);
#endif
	if (WC->urlstrings == NULL)
		WC->urlstrings = NewHash(1, NULL);

	/* Form fields */
	if ( (length > 0) && (IsEmptyStr(cbtype)) ) {
		u = (urlcontent *) malloc(sizeof(urlcontent));
		
		safestrncpy(u->url_key, name, sizeof(u->url_key));
		u->url_data = NewStrBufPlain(content, length);
		
		Put(WC->urlstrings, u->url_key, strlen(u->url_key), u, free_url);
#ifdef DEBUG_URLSTRINGS
		lprintf(9, "Key: <%s> len: [%ld] Data: <%s>\n", 
			u->url_key, 
			StrLength(u->url_data), 
			ChrPtr(u->url_data));
#endif
	}

	/** Uploaded files */
	if ( (length > 0) && (!IsEmptyStr(cbtype)) ) {
		WC->upload = malloc(length);
		if (WC->upload != NULL) {
			WC->upload_length = length;
			safestrncpy(WC->upload_filename, filename,
					sizeof(WC->upload_filename));
			safestrncpy(WC->upload_content_type, cbtype,
					sizeof(WC->upload_content_type));
			memcpy(WC->upload, content, length);
		}
		else {
			lprintf(3, "malloc() failed: %s\n", strerror(errno));
		}
	}

}



void PutBstr(const char *key, long keylen, StrBuf *Value)
{
	urlcontent *u;

	if(keylen > sizeof(u->url_key)) {
		lprintf(1, "URLkey to long! [%s]", key);
		FreeStrBuf(&Value);
		return;
	}
	u = (urlcontent*)malloc(sizeof(urlcontent));
	memcpy(u->url_key, key, keylen + 1);
	u->url_data = Value;
	Put(WC->urlstrings, u->url_key, keylen, u, free_url);
}



int ConditionalBstr(StrBuf *Target, WCTemplputParams *TP)
{
	if(TP->Tokens->nParameters == 3)
		return HaveBstr(TKEY(2));
	else {
		if (TP->Tokens->Params[3]->Type == TYPE_LONG)
			return LBstr(TKEY(2)) == TP->Tokens->Params[3]->lvalue;
		else 
			return strcmp(Bstr(TKEY(2)),
				      TP->Tokens->Params[3]->Start) == 0;
	}
}

void tmplput_bstr(StrBuf *Target, WCTemplputParams *TP)
{
	const StrBuf *Buf = SBstr(TKEY(0));
	if (Buf != NULL)
		StrBufAppendTemplate(Target, TP, Buf, 1);
}

void diagnostics(void)
{
	output_headers(1, 1, 1, 0, 0, 0);
	wprintf("Session: %d<hr />\n", WC->wc_session);
	wprintf("Command: <br /><PRE>\n");
	StrEscPuts(WC->UrlFragment1);
	wprintf("<br />\n");
	StrEscPuts(WC->UrlFragment2);
	wprintf("<br />\n");
	StrEscPuts(WC->UrlFragment3);
	wprintf("</PRE><hr />\n");
	wprintf("Variables: <br /><PRE>\n");
	dump_vars();
	wprintf("</PRE><hr />\n");
	wDumpContent(1);
}


void tmplput_url_part(StrBuf *Target, WCTemplputParams *TP)
{
	StrBuf *UrlBuf;
	wcsession *WCC = WC;
	
	if (WCC != NULL) {
		if (TP->Tokens->Params[0]->lvalue == 0)
			UrlBuf = WCC->UrlFragment1;
		else if (TP->Tokens->Params[0]->lvalue == 1)
			UrlBuf = WCC->UrlFragment2;
		else
			UrlBuf = WCC->UrlFragment3;
		if (UrlBuf == NULL)  {
			LogTemplateError(Target, "urlbuf", ERR_PARM1, TP, "not set.");
		}
		StrBufAppendTemplate(Target, TP, UrlBuf, 2);
	}
}


void 
InitModule_PARAMHANDLING
(void)
{
	WebcitAddUrlHandler(HKEY("diagnostics"), diagnostics, NEED_URL);

	RegisterConditional(HKEY("COND:BSTR"), 1, ConditionalBstr, CTX_NONE);
	RegisterNamespace("BSTR", 1, 2, tmplput_bstr, CTX_NONE);
	RegisterNamespace("URLPART", 1, 2, tmplput_url_part, CTX_NONE);
}
