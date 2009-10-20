/*
 * $Id$
 *
 * Entry point for GroupDAV functions
 *
 */

#include "webcit.h"
#include "webserver.h"
#include "groupdav.h"


/*
 * Output HTTP headers which are common to all requests.
 *
 * Please observe that we don't use the usual output_headers()
 * and wDumpContent() functions in the GroupDAV subsystem, so we
 * do our own header stuff here.
 *
 */
void groupdav_common_headers(void) {
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
void groupdav_main(void)
{
	wcsession *WCC = WC;
	int i, len;

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
	 * The OPTIONS method is not required by GroupDAV.  This is an
	 * experiment to determine what might be involved in supporting
	 * other variants of DAV in the future.
	 */
	case eOPTIONS:
		groupdav_options();
		break;


	/*
	 * The PROPFIND method is basically used to list all objects in a
	 * room, or to list all relevant rooms on the server.
	 */
	case ePROPFIND:
		groupdav_propfind();
		break;

	/*
	 * The GET method is used for fetching individual items.
	 */
	case eGET:
		groupdav_get();
		break;
	
	/*
	 * The PUT method is used to add or modify items.
	 */
	case ePUT:
		groupdav_put();
		break;
	
	/*
	 * The DELETE method kills, maims, and destroys.
	 */
	case eDELETE:
		groupdav_delete();
		break;
	default:

	/*
	 * Couldn't find what we were looking for.  Die in a car fire.
	 */
		hprintf("HTTP/1.1 501 Method not implemented\r\n");
		groupdav_common_headers();
		hprintf("Content-Type: text/plain\r\n");
		wprintf("GroupDAV method \"%s\" is not implemented.\r\n",
			ReqStrs[WCC->Hdr->HR.eReqType]);
		end_burst();
	}
}


/*
 * Output our host prefix for globally absolute URL's.
 */  
void groupdav_identify_host(void) {
	wcsession *WCC = WC;

	if (StrLength(WCC->Hdr->HR.http_host)!=0) {
		wprintf("%s://%s",
			(is_https ? "https" : "http"),
			ChrPtr(WCC->Hdr->HR.http_host));
	}
}

/*
 * Output our host prefix for globally absolute URL's.
 */  
void groupdav_identify_hosthdr(void) {
	wcsession *WCC = WC;

	if (StrLength(WCC->Hdr->HR.http_host)!=0) {
		hprintf("%s://%s",
			(is_https ? "https" : "http"),
			ChrPtr(WCC->Hdr->HR.http_host));
	}
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

void 
InitModule_GROUPDAV
(void)
{
	WebcitAddUrlHandler(HKEY("groupdav"), "", 0, groupdav_main, XHTTP_COMMANDS|COOKIEUNNEEDED|FORCE_SESSIONCLOSE);
	RegisterHeaderHandler(HKEY("IF-MATCH"), Header_HandleIfMatch);
	RegisterHeaderHandler(HKEY("DEPTH"), Header_HandleDepth);

}
