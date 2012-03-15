/*
 * Entry point for GroupDAV functions
 *
 * Copyright (c) 2005-2012 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 3.
 * 
 * 
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * 
 * 
 * 
 */

#include "webcit.h"
#include "webserver.h"
#include "dav.h"

extern HashList *HandlerHash;

HashList *DavNamespaces = NULL;

/*
 * Output HTTP headers which are common to all requests.
 *
 * Please observe that we don't use the usual output_headers()
 * and wDumpContent() functions in the GroupDAV subsystem, so we
 * do our own header stuff here.
 *
 */
void dav_common_headers(void) {
	hprintf(
		"Server: %s / %s\r\n"
		"Connection: close\r\n",
		PACKAGE_STRING, ChrPtr(WC->serv_info->serv_software)
	);
}



/*
 * string conversion function
 */
void euid_escapize(char *target, const char *source) {
	int i, len;
	int target_length = 0;

	strcpy(target, "");
	len = strlen(source);
	for (i=0; i<len; ++i) {
		if ( (isalnum(source[i])) || (source[i]=='-') || (source[i]=='_') ) {
			target[target_length] = source[i];
			target[++target_length] = 0;
		}
		else {
			sprintf(&target[target_length], "=%02X", (0xFF & source[i]));
			target_length += 3;
		}
	}
}

/*
 * string conversion function
 */
void euid_unescapize(char *target, const char *source) {
	int a, b, len;
	char hex[3];
	int target_length = 0;

	strcpy(target, "");

	len = strlen(source);
	for (a = 0; a < len; ++a) {
		if (source[a] == '=') {
			hex[0] = source[a + 1];
			hex[1] = source[a + 2];
			hex[2] = 0;
			b = 0;
			b = decode_hex(hex);
			target[target_length] = b;
			target[++target_length] = 0;
			a += 2;
		}
		else {
			target[target_length] = source[a];
			target[++target_length] = 0;
		}
	}
}




/*
 * Main entry point for GroupDAV requests
 */
void dav_main(void)
{
	wcsession *WCC = WC;
	int i, len;

	syslog(LOG_DEBUG, "dav_main() called, logged_in=%d", WCC->logged_in );

	StrBufUnescape(WCC->Hdr->HR.ReqLine, 0);
	StrBufStripSlashes(WCC->Hdr->HR.ReqLine, 0);

	/*
	 * If there's an If-Match: header, strip out the quotes if present, and
	 * then if all that's left is an asterisk, make it go away entirely.
	 */
	len = StrLength(WCC->Hdr->HR.dav_ifmatch);
	if (len > 0) {
		StrBufTrim(WCC->Hdr->HR.dav_ifmatch);
		if (ChrPtr(WCC->Hdr->HR.dav_ifmatch)[0] == '\"') {
			StrBufCutLeft(WCC->Hdr->HR.dav_ifmatch, 1);
			len --;
			for (i=0; i<len; ++i) {
				if (ChrPtr(WCC->Hdr->HR.dav_ifmatch)[i] == '\"') {
					StrBufCutAt(WCC->Hdr->HR.dav_ifmatch, i, NULL);
					len = StrLength(WCC->Hdr->HR.dav_ifmatch);
				}
			}
		}
		if (!strcmp(ChrPtr(WCC->Hdr->HR.dav_ifmatch), "*")) {
			FlushStrBuf(WCC->Hdr->HR.dav_ifmatch);
		}
	}

	switch (WCC->Hdr->HR.eReqType)
	{
	/*
	 * The OPTIONS method is not required by GroupDAV but it will be
	 * needed for future implementations of other DAV-based protocols.
	 */
	case eOPTIONS:
		dav_options();
		break;

	/*
	 * The PROPFIND method is basically used to list all objects in a
	 * room, or to list all relevant rooms on the server.
	 */
	case ePROPFIND:
		dav_propfind();
		break;

	/*
	 * The GET method is used for fetching individual items.
	 */
	case eGET:
		dav_get();
		break;
	
	/*
	 * The PUT method is used to add or modify items.
	 */
	case ePUT:
		dav_put();
		break;
	
	/*
	 * The DELETE method kills, maims, and destroys.
	 */
	case eDELETE:
		dav_delete();
		break;

	/*
	 * The REPORT method tells us that Mike Shaver is a self-righteous asshole.
	 */
	case eREPORT:
		dav_report();
		break;

	default:
	/*
	 * Couldn't find what we were looking for.  Die in a car fire.
	 */
		hprintf("HTTP/1.1 501 Method not implemented\r\n");
		dav_common_headers();
		hprintf("Content-Type: text/plain\r\n");
		wc_printf("GroupDAV method \"%s\" is not implemented.\r\n",
			ReqStrs[WCC->Hdr->HR.eReqType]);
		end_burst();
	}
}


/*
 * Output our host prefix for globally absolute URL's.
 */  
void dav_identify_host(void) {
	wc_printf("%s", ChrPtr(site_prefix));
}


void tmplput_dav_HOSTNAME(StrBuf *Target, WCTemplputParams *TP) 
{
	StrBufAppendPrintf(Target, "%s", ChrPtr(site_prefix));
}

/*
 * Output our host prefix for globally absolute URL's.
 */  
void dav_identify_hosthdr(void) {
	hprintf("%s", ChrPtr(site_prefix));
}


void Header_HandleIfMatch(StrBuf *Line, ParsedHttpHdrs *hdr)
{
	hdr->HR.dav_ifmatch = Line;
}
	

void Header_HandleDepth(StrBuf *Line, ParsedHttpHdrs *hdr)
{
	if (!strcasecmp(ChrPtr(Line), "infinity")) {
		hdr->HR.dav_depth = 32767;
	}
	else if (strcmp(ChrPtr(Line), "0") == 0) {
		hdr->HR.dav_depth = 0;
	}
	else if (strcmp(ChrPtr(Line), "1") == 0) {
		hdr->HR.dav_depth = 1;
	}
}


int Conditional_DAV_DEPTH(StrBuf *Target, WCTemplputParams *TP)
{
	return WC->Hdr->HR.dav_depth == GetTemplateTokenNumber(Target, TP, 2, 0);
}


void RegisterDAVNamespace(const char * UrlString, 
			  long UrlSLen, 
			  const char *DisplayName, 
			  long dslen, 
			  WebcitHandlerFunc F, 
			  WebcitRESTDispatchID RID,
			  long Flags)
{
	void *vHandler;

	/* first put it in... */
	WebcitAddUrlHandler(UrlString, UrlSLen, DisplayName, dslen, F, Flags|PARSE_REST_URL);
	/* get it out again... */
	GetHash(HandlerHash, UrlString, UrlSLen, &vHandler);
	((WebcitHandler*)vHandler)->RID = RID;
	/* and keep a copy of it, so we can compare it later */
	Put(DavNamespaces, UrlString, UrlSLen, vHandler, reference_free_handler);
}


int Conditional_DAV_NS(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;
	void *vHandler;
	const char *NS;
	long NSLen;

	GetTemplateTokenString(NULL, TP, 2, &NS, &NSLen);
	GetHash(HandlerHash, NS, NSLen, &vHandler);
	return WCC->Hdr->HR.Handler == vHandler;
}


int Conditional_DAV_NSCURRENT(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;
	void *vHandler;

	vHandler = CTX;
	return WCC->Hdr->HR.Handler == vHandler;
}


void tmplput_DAV_NAMESPACE(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;

	if (TP->Filter.ContextType == CTX_DAVNS) {
		WebcitHandler *H;
		H = (WebcitHandler*) CTX;
		StrBufAppendTemplate(Target, TP, H->Name, 0);
	}
	else if (WCC->Hdr->HR.Handler != NULL) {
		StrBufAppendTemplate(Target, TP, WCC->Hdr->HR.Handler->Name, 0);
	}
}


int GroupdavDispatchREST(RESTDispatchID WhichAction, int IgnoreFloor)
{
	wcsession *WCC = WC;
	void *vDir;
	
	switch(WhichAction){
	case ExistsID:
		GetHash(WCC->Directory, IKEY(WCC->ThisRoom->nRoomNameParts + 1), &vDir);
		return locate_message_by_uid(ChrPtr((StrBuf*)vDir)) != -1;
		/* TODO: remember euid */
	case PutID:
	case DeleteID:
		break;


	}
	return 0;
}


void
ServerStartModule_DAV
(void)
{

	DavNamespaces = NewHash(1, NULL);
}


void 
ServerShutdownModule_DAV
(void)
{
	DeleteHash(&DavNamespaces);
}


void 
InitModule_GROUPDAV
(void)
{
	RegisterDAVNamespace(HKEY("groupdav"), HKEY("GroupDAV"), 
			     dav_main, GroupdavDispatchREST, 
			     XHTTP_COMMANDS|COOKIEUNNEEDED|FORCE_SESSIONCLOSE
	);

	RegisterNamespace("DAV:HOSTNAME", 0, 0, tmplput_dav_HOSTNAME, NULL, CTX_NONE);

	RegisterConditional(HKEY("COND:DAV:NS"), 0, Conditional_DAV_NS,  CTX_NONE);

	RegisterIterator("DAV:NS", 0, DavNamespaces, NULL, 
			 NULL, NULL, CTX_DAVNS, CTX_NONE, IT_NOFLAG
	);

	RegisterConditional(HKEY("COND:DAV:NSCURRENT"), 0, Conditional_DAV_NSCURRENT,  CTX_DAVNS);
	RegisterNamespace("DAV:NAMESPACE", 0, 1, tmplput_DAV_NAMESPACE, NULL, CTX_NONE);

	RegisterHeaderHandler(HKEY("IF-MATCH"), Header_HandleIfMatch);
	RegisterHeaderHandler(HKEY("DEPTH"), Header_HandleDepth);
	RegisterConditional(HKEY("COND:DAV:DEPTH"), 1, Conditional_DAV_DEPTH,  CTX_NONE);
}
