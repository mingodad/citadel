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
		PACKAGE_STRING, ChrPtr(serv_info.serv_software)
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
			sscanf(hex, "%02x", &b);
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
		   StrBuf *DavMethod,
		   StrBuf *dav_content_type,
		   int dav_content_length,
		   StrBuf *dav_content,
		   int Offset
) {
	void *vLine;
	char dav_ifmatch[256];
	int dav_depth;
	char *ds;
	int i, len;

	strcpy(dav_ifmatch, "");
	dav_depth = 0;

	if (IsEmptyStr(WC->http_host) &&
	    GetHash(HTTPHeaders, HKEY("HOST"), &vLine) && 
	    (vLine != NULL)) {
		safestrncpy(WC->http_host, ChrPtr((StrBuf*)vLine),
			    sizeof WC->http_host);
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
			ChrPtr(serv_info.serv_humannode));
		hprintf("Content-Length: 0\r\n");
		end_burst();
		return;
	}

//	extract_token(dav_method, req->line, 0, ' ', sizeof dav_method);
//	extract_token(dav_pathname, req->line, 1, ' ', sizeof dav_pathname);
	//// TODO unescape_input(dav_pathname);

	/* If the request does not begin with "/groupdav", prepend it.  If
	 * we happen to introduce a double-slash, that's ok; we'll strip it
	 * in the next step.
	 * 
	 * (THIS IS DISABLED BECAUSE WE ARE NOW TRYING TO DO REAL DAV.)
	 *
	if (strncasecmp(dav_pathname, "/groupdav", 9)) {
		char buf[512];
		snprintf(buf, sizeof buf, "/groupdav/%s", dav_pathname);
		safestrncpy(dav_pathname, buf, sizeof dav_pathname);
	}
	 *
	 */
	
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

	/*
	 * The OPTIONS method is not required by GroupDAV.  This is an
	 * experiment to determine what might be involved in supporting
	 * other variants of DAV in the future.
	 */
	if (!strcasecmp(ChrPtr(DavMethod), "OPTIONS")) {
		groupdav_options(DavPathname);
		return;
	}

	/*
	 * The PROPFIND method is basically used to list all objects in a
	 * room, or to list all relevant rooms on the server.
	 */
	if (!strcasecmp(ChrPtr(DavMethod), "PROPFIND")) {
		groupdav_propfind(DavPathname, dav_depth,
				  dav_content_type, dav_content, 
				  Offset);
		return;
	}

	/*
	 * The GET method is used for fetching individual items.
	 */
	if (!strcasecmp(ChrPtr(DavMethod), "GET")) {
		groupdav_get(DavPathname);
		return;
	}

	/*
	 * The PUT method is used to add or modify items.
	 */
	if (!strcasecmp(ChrPtr(DavMethod), "PUT")) {
		groupdav_put(DavPathname, dav_ifmatch,
			     ChrPtr(dav_content_type), dav_content, 
			     Offset);
		return;
	}

	/*
	 * The DELETE method kills, maims, and destroys.
	 */
	if (!strcasecmp(ChrPtr(DavMethod), "DELETE")) {
		groupdav_delete(DavPathname, dav_ifmatch);
		return;
	}

	/*
	 * Couldn't find what we were looking for.  Die in a car fire.
	 */
	hprintf("HTTP/1.1 501 Method not implemented\r\n");
	groupdav_common_headers();
	hprintf("Content-Type: text/plain\r\n");
	wprintf("GroupDAV method \"%s\" is not implemented.\r\n",
		ChrPtr(DavMethod));
	end_burst();
}


/*
 * Output our host prefix for globally absolute URL's.
 */  
void groupdav_identify_host(void) {
	if (!IsEmptyStr(WC->http_host)) {
		wprintf("%s://%s",
			(is_https ? "https" : "http"),
			WC->http_host);
	}
}
