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
void groupdav_main(HashList *HTTPHeaders,
		   StrBuf *DavPathname,
		   StrBuf *dav_content_type,
		   int dav_content_length,
		   StrBuf *dav_content,
		   int Offset
) {
	wcsession *WCC = WC;
	void *vLine;
	char dav_ifmatch[256];
	int dav_depth;
	char *ds;
	int i, len;

	strcpy(dav_ifmatch, "");
	dav_depth = 0;

	if ((StrLength(WCC->http_host) == 0) &&
	    GetHash(HTTPHeaders, HKEY("HOST"), &vLine) && 
	    (vLine != NULL)) {
		WCC->http_host = (StrBuf*)vLine;
	}
	if (GetHash(HTTPHeaders, HKEY("IF-MATCH"), &vLine) && 
	    (vLine != NULL)) {
		safestrncpy(dav_ifmatch, ChrPtr((StrBuf*)vLine),
			    sizeof dav_ifmatch);
	}
	if (GetHash(HTTPHeaders, HKEY("DEPTH"), &vLine) && 
	    (vLine != NULL)) {
		if (!strcasecmp(ChrPtr((StrBuf*)vLine), "infinity")) {
			dav_depth = 32767;
		}
		else if (strcmp(ChrPtr((StrBuf*)vLine), "0") == 0) {
			dav_depth = 0;
		}
		else if (strcmp(ChrPtr((StrBuf*)vLine), "1") == 0) {
			dav_depth = 1;
		}
	}

	if (!WC->logged_in) {
		hprintf("HTTP/1.1 401 Unauthorized\r\n");
		groupdav_common_headers();
		hprintf("WWW-Authenticate: Basic realm=\"%s\"\r\n",
			ChrPtr(WCC->serv_info->serv_humannode));
		hprintf("Content-Length: 0\r\n");
		end_burst();
		return;
	}

	StrBufUnescape(DavPathname, 0);

	/* Remove any stray double-slashes in pathname */
	while (ds=strstr(ChrPtr(DavPathname), "//"), ds != NULL) {
		strcpy(ds, ds+1);
	}

	/*
	 * If there's an If-Match: header, strip out the quotes if present, and
	 * then if all that's left is an asterisk, make it go away entirely.
	 */
	len = strlen(dav_ifmatch);
	if (len > 0) {
		stripltlen(dav_ifmatch, &len);
		if (dav_ifmatch[0] == '\"') {
			memmove (dav_ifmatch, &dav_ifmatch[1], len);
			len --;
			for (i=0; i<len; ++i) {
				if (dav_ifmatch[i] == '\"') {
					dav_ifmatch[i] = 0;
					len = i - 1;
				}
			}
		}
		if (!strcmp(dav_ifmatch, "*")) {
			strcpy(dav_ifmatch, "");
		}
	}

	switch (WCC->eReqType)
	{
	/*
	 * The OPTIONS method is not required by GroupDAV.  This is an
	 * experiment to determine what might be involved in supporting
	 * other variants of DAV in the future.
	 */
	case eOPTIONS:
		groupdav_options(DavPathname);
		break;


	/*
	 * The PROPFIND method is basically used to list all objects in a
	 * room, or to list all relevant rooms on the server.
	 */
	case ePROPFIND:
		groupdav_propfind(DavPathname, dav_depth,
				  dav_content_type, dav_content, 
				  Offset);
		break;

	/*
	 * The GET method is used for fetching individual items.
	 */
	case eGET:
		groupdav_get(DavPathname);
		break;
	
	/*
	 * The PUT method is used to add or modify items.
	 */
	case ePUT:
		groupdav_put(DavPathname, dav_ifmatch,
			     ChrPtr(dav_content_type), dav_content, 
			     Offset);
		break;
	
	/*
	 * The DELETE method kills, maims, and destroys.
	 */
	case eDELETE:
		groupdav_delete(DavPathname, dav_ifmatch);
		break;
	default:

	/*
	 * Couldn't find what we were looking for.  Die in a car fire.
	 */
		hprintf("HTTP/1.1 501 Method not implemented\r\n");
		groupdav_common_headers();
		hprintf("Content-Type: text/plain\r\n");
		wprintf("GroupDAV method \"%s\" is not implemented.\r\n",
			ReqStrs[WCC->eReqType]);
		end_burst();
	}
}


/*
 * Output our host prefix for globally absolute URL's.
 */  
void groupdav_identify_host(void) {
	wcsession *WCC = WC;

	if (StrLength(WCC->http_host)!=0) {
		wprintf("%s://%s",
			(is_https ? "https" : "http"),
			ChrPtr(WCC->http_host));
	}
}
