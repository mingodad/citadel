/*
 * parse urlparts and post data
 *
 * Copyright (c) 1996-2013 by the citadel.org team
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

/* uncomment to see all parameters sent to the server by the browser. */
/* #define DEBUG_URLSTRINGS */


void free_url(void *U)
{
	urlcontent *u = (urlcontent*) U;
	FreeStrBuf(&u->url_data);
	if (u->sub != NULL) {
		DeleteHash(&u->sub);
	}
	free(u);
}

void PutSubstructUrlKey(HashList *list, urlcontent *u, char **keys, long *lengths, int max, int which){
	void *vUrl;
	urlcontent *subu;
	HashList *thisList = list;
	if (GetHash(list, keys[which], lengths[which], &vUrl) &&
	    (vUrl != NULL))
	{
		subu = (urlcontent*) vUrl;
		if (subu->sub == NULL) {
			subu->sub = NewHash(1, NULL);
		}
		thisList = subu->sub;
	} 
	else if (which < max) {
		subu = (urlcontent *) malloc(sizeof(urlcontent));
		
		memcpy(subu->url_key, keys[which], lengths[which]);
		subu->klen = lengths[which];
		subu->url_data = NULL;
		subu->sub = NewHash(1, NULL);
		
		Put(list, subu->url_key, subu->klen, subu, free_url);
		thisList = subu->sub;
	}
	if (which >= max) {
		Put(thisList, keys[which], lengths[which], u, free_url);
	}
	else {
		PutSubstructUrlKey(subu->sub, u, keys, lengths, max, which + 1);
	}
}

void PutUrlKey(HashList *urlstrings, urlcontent *u, int have_colons) {
	if (have_colons == 0) {
		Put(urlstrings, u->url_key, u->klen, u, free_url);
	}
	else {
		char *keys[10];
		long lengths[10];
		int i = 0;
		char *pch;
		char *pchs;
		char *pche;

		memset(&keys, 0, sizeof(keys));
		memset(&lengths, 0, sizeof(lengths));
		pchs = pch = u->url_key;
		pche = u->url_key + u->klen;
		while ((i < 10) && (pch <= pche)) {
			if ((have_colons == 2) &&
			    (*pch == '%') &&
			    (*(pch + 1) == '3') && 
			    ((*(pch + 2) == 'A') ||
			     (*(pch + 1) == 'a')
				    ))
			{
				*pch = '\0';

				if (i == 0) {
					/* Separate the toplevel key : */
					u->klen = pch - pchs;
				}

				/* sub-section: */
				keys[i] = pchs;
				lengths[i] = pch - pchs;

				pch += 3;

				pchs = pch;
				i++;
			}
			else if ((have_colons == 1) &&
				 (*pch == ':')) {
				*pch = '\0';
				if (i == 0) {
					/* Separate the toplevel key : */
					u->klen = pch - pchs;
				}
				/* sub-section: */
				keys[i] = pchs;
				lengths[i] = pch - pchs;
			
				pch++;
				pchs = pch;
				i++;
			}
			else if (pch == pche){
				/* sub-section: */
				keys[i] = pchs;
				lengths[i] = pch - pchs;
				i++;
				break;
			}
			else {
				pch ++;
			}
		}
		
		PutSubstructUrlKey(urlstrings, u, keys, lengths, i - 1, 0);
	}
}

/*
 * Extract variables from the URL.
 */
void ParseURLParams(StrBuf *url)
{
	const char *aptr, *bptr, *eptr, *up = NULL;
	int len, keylen = 0;
	urlcontent *u = NULL;
	wcsession *WCC = WC;

	if (WCC->Hdr->urlstrings == NULL) {
		WCC->Hdr->urlstrings = NewHash(1, NULL);
	}
	eptr = ChrPtr(url) + StrLength(url);
	up = ChrPtr(url);
	while ((up < eptr) && (!IsEmptyStr(up))) {
		int have_colon = 0;
		aptr = up;
		while ((aptr < eptr) && (*aptr != '\0') && (*aptr != '=')) {
			if (*aptr == ':') {
				have_colon = 1;
			}
			else if ((*aptr == '%') &&
				 (*(aptr + 1) == '3') && 
				 ((*(aptr + 2) == 'A') ||
				  (*(aptr + 1) == 'a')
					 ))
			{
				have_colon = 2;
			}
			aptr++;
		}
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
		if (keylen > sizeof(u->url_key)) {
			syslog(LOG_WARNING, "%s:%d: invalid url_key of size %d in string size %ld",
				__FILE__, __LINE__, keylen, sizeof(u->url_key)
			);
		}

		if (keylen < 0) {
			syslog(LOG_WARNING, "%s:%d: invalid url_key of size %d", __FILE__, __LINE__, keylen);
			free(u);
			return;
		}
		
		u = (urlcontent *) malloc(sizeof(urlcontent));
		memcpy(u->url_key, up, keylen);
		u->url_key[keylen] = '\0';
		u->klen = keylen;
		u->sub = NULL;

		if (strncmp(u->url_key, "__", 2) != 0)
		{
			len = bptr - aptr;
			u->url_data = NewStrBufPlain(aptr, len);
			StrBufUnescape(u->url_data, 1);
#ifdef DEBUG_URLSTRINGS
			syslog(LOG_DEBUG, "%s = [%d]  %s\n", 
				u->url_key, 
				StrLength(u->url_data), 
				ChrPtr(u->url_data)); 
#endif
			PutUrlKey(WCC->Hdr->urlstrings, u, have_colon);
		}
		else {
			len = bptr - aptr;
			u->url_data = NewStrBufPlain(aptr, len);
			StrBufUnescape(u->url_data, 1);
			syslog(LOG_WARNING, "REJECTED because of __ is internal only: %s = [%d]  %s\n", 
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
 * Return a sub array that was separated by a colon:
 */
HashList* getSubStruct(const char *key, size_t keylen)
{
	void *U;

	if ((WC->Hdr->urlstrings != NULL) && 
	    GetHash(WC->Hdr->urlstrings, key, strlen(key), &U))
		return ((urlcontent *)U)->sub;
	else	
		return NULL;
}


/*
 * Return the value of a variable of a substruct provided by getSubStruct
 */
const char *XSubBstr(HashList *sub, const char *key, size_t keylen, size_t *len)
{
	void *U;

	if ((sub != NULL) && 
	    GetHash(sub, key, keylen, &U)) {
		*len = StrLength(((urlcontent *)U)->url_data);
		return ChrPtr(((urlcontent *)U)->url_data);
	}
	else {
		*len = 0;
		return ("");
	}
}

const char *SubBstr(HashList *sub, const char *key, size_t keylen)
{
	void *U;

	if ((sub != NULL) && 
	    GetHash(sub, key, keylen, &U)) {
		return ChrPtr(((urlcontent *)U)->url_data);
	}
	else	
		return ("");
}

const StrBuf *SSubBstr(HashList *sub, const char *key, size_t keylen)
{
	void *U;

	if ((sub != NULL) && 
	    GetHash(sub, key, keylen, &U)) {
		return ((urlcontent *)U)->url_data;
	}
	else	
		return NULL;
}

long LSubBstr(HashList *sub, const char *key, size_t keylen)
{
	void *U;

	if ((sub != NULL) && 
	    GetHash(sub, key, keylen, &U)) {
		return StrTol(((urlcontent *)U)->url_data);
	}
	else	
		return (0);
}

int ISubBstr(HashList *sub, const char *key, size_t keylen)
{
	void *U;

	if ((sub != NULL) && 
	    GetHash(sub, key, keylen, &U)) {
		return StrTol(((urlcontent *)U)->url_data);
	}
	else	
		return (0);
}

int HaveSubBstr(HashList *sub, const char *key, size_t keylen)
{
	void *U;

	if ((sub != NULL) && 
	    GetHash(sub, key, keylen, &U)) {
		return (StrLength(((urlcontent *)U)->url_data) != 0);
	}
	else	
		return (0);
}

int YesSubBstr(HashList *sub, const char *key, size_t keylen)
{
	void *U;

	if ((sub != NULL) && 
	    GetHash(sub, key, keylen, &U)) {
		return strcmp( ChrPtr(((urlcontent *)U)->url_data), "yes") == 0;
	}
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
	syslog(LOG_DEBUG, "upload_handler() name=%s, type=%s, len="SIZE_T_FMT, name, cbtype, length);
#endif
	if (WCC->Hdr->urlstrings == NULL)
		WCC->Hdr->urlstrings = NewHash(1, NULL);

	/* Form fields */
	if ( (length > 0) && (IsEmptyStr(cbtype)) ) {
		u = (urlcontent *) malloc(sizeof(urlcontent));
		
		keylen = safestrncpy(u->url_key, name, sizeof(u->url_key));
		u->url_data = NewStrBufPlain(content, length);
		u->klen = keylen;
		u->sub = NULL;
		
		if (strncmp(u->url_key, "__", 2) != 0)
		{
			PutUrlKey(WCC->Hdr->urlstrings, u, (strchr(u->url_key, ':') != NULL));
		}
		else {
			syslog(LOG_INFO, "REJECTED because of __ is internal only: %s = [%d]  %s\n", 
				u->url_key, 
				StrLength(u->url_data), 
				ChrPtr(u->url_data)); 
			
			free_url(u);
		}
#ifdef DEBUG_URLSTRINGS
		syslog(LOG_DEBUG, "Key: <%s> len: [%d] Data: <%s>", 
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
		syslog(LOG_DEBUG, "File: <%s> len: [%ld]", filename, (long int)length);
#endif
		
	}

}

void PutBstr(const char *key, long keylen, StrBuf *Value)
{
	urlcontent *u;

	if(keylen >= sizeof(u->url_key)) {
		syslog(LOG_WARNING, "%s:%d: invalid url_key of size %ld", __FILE__, __LINE__, keylen);
		FreeStrBuf(&Value);
		return;
	}
	u = (urlcontent*)malloc(sizeof(urlcontent));
	memcpy(u->url_key, key, keylen + 1);
	u->klen = keylen;
	u->url_data = Value;
	u->sub = NULL;
	Put(WC->Hdr->urlstrings, u->url_key, keylen, u, free_url);
}
void PutlBstr(const char *key, long keylen, long Value)
{
	StrBuf *Buf;

	Buf = NewStrBufPlain(NULL, sizeof(long) * 16);
	StrBufPrintf(Buf, "%ld", Value);
	PutBstr(key, keylen, Buf);
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

typedef struct __BstrPair {
	StrBuf *x;
	StrBuf *y;
}BstrPair;
CtxType CTX_BSTRPAIRS = CTX_NONE;
void HFreeBstrPair(void *pv)
{
	BstrPair *p = (BstrPair*) pv;
	FreeStrBuf(&p->x);
	FreeStrBuf(&p->y);
	free(pv);
}

HashList *iterate_GetBstrPairs(StrBuf *Target, WCTemplputParams *TP)
{
	StrBuf *X, *Y;
        const char *ch = NULL;
        long len;
	const StrBuf *TheBStr;
	BstrPair *OnePair;
        HashList *List;
	const char *Pos = NULL;
	int i = 0;

	if (HaveTemplateTokenString(NULL, TP, 2, &ch, &len))
        {
                GetTemplateTokenString(Target, TP, 2, &ch, &len);
        }
	else 
	{
		return NULL;
	}

	TheBStr = SBstr(ch, len);
	if ((TheBStr == NULL) || (StrLength(TheBStr) == 0))
		return NULL;
	List = NewHash(1, NULL);
	while (Pos != StrBufNOTNULL)
	{
		X = NewStrBufPlain(NULL, StrLength(TheBStr));
		StrBufExtract_NextToken(X, TheBStr, &Pos, '|');
		if (Pos == StrBufNOTNULL) {
			FreeStrBuf(&X);
			DeleteHash(&List);
			return NULL;
		}
		Y = NewStrBufPlain(NULL, StrLength(TheBStr));
		StrBufExtract_NextToken(Y, TheBStr, &Pos, '|');
		OnePair = (BstrPair*)malloc(sizeof(BstrPair));
		OnePair->x = X;
		OnePair->y = Y;
		Put(List, IKEY(i), OnePair, HFreeBstrPair);
		i++;
	}
	return List;
}


void tmplput_bstr_pair(StrBuf *Target, WCTemplputParams *TP, int XY)
{
	BstrPair *Pair = (BstrPair*) CTX(CTX_BSTRPAIRS);

	StrBufAppendTemplate(Target, TP, (XY)?Pair->y:Pair->x, 0);
}

void tmplput_bstr_pair_x(StrBuf *Target, WCTemplputParams *TP)
{	tmplput_bstr_pair(Target, TP, 0); }
void tmplput_bstr_pair_y(StrBuf *Target, WCTemplputParams *TP)
{	tmplput_bstr_pair(Target, TP, 1); }

void 
InitModule_PARAMHANDLING
(void)
{
	RegisterCTX(CTX_BSTRPAIRS);
	WebcitAddUrlHandler(HKEY("diagnostics"), "", 0, diagnostics, NEED_URL);

	RegisterIterator("ITERATE:BSTR:PAIR", 1, NULL, iterate_GetBstrPairs, NULL, DeleteHash, CTX_BSTRPAIRS, CTX_NONE, IT_NOFLAG);
	RegisterNamespace("BSTR:PAIR:X", 1, 2, tmplput_bstr_pair_x, NULL, CTX_BSTRPAIRS);
	RegisterNamespace("BSTR:PAIR:Y", 1, 2, tmplput_bstr_pair_y, NULL, CTX_BSTRPAIRS);

	RegisterConditional("COND:BSTR", 1, ConditionalBstr, CTX_NONE);
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
