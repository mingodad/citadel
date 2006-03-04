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
	wprintf(
		"Server: %s / %s\r\n"
		"Connection: close\r\n",
		SERVER, serv_info.serv_software
	);
}



/*
 * string conversion function
 */
void euid_escapize(char *target, char *source) {
	int i;
	int target_length = 0;

	strcpy(target, "");
	for (i=0; i<strlen(source); ++i) {
		if ( (isalnum(source[i])) || (source[i]=='-') || (source[i]=='_') ) {
			target[target_length] = source[i];
			target[++target_length] = 0;
		}
		else {
			sprintf(&target[target_length], "=%02X", source[i]);
			target_length += 3;
		}
	}
}

/*
 * string conversion function
 */
void euid_unescapize(char *target, char *source) {
	int a, b;
	char hex[3];
	int target_length = 0;

	strcpy(target, "");

	for (a = 0; a < strlen(source); ++a) {
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
void groupdav_main(struct httprequest *req,
			char *dav_content_type,
			int dav_content_length,
			char *dav_content
) {
	struct httprequest *rptr;
	char dav_method[256];
	char dav_pathname[256];
	char dav_ifmatch[256];
	int dav_depth;
	char *ds;
	int i;

	strcpy(dav_method, "");
	strcpy(dav_pathname, "");
	strcpy(dav_ifmatch, "");
	dav_depth = 0;

	for (rptr=req; rptr!=NULL; rptr=rptr->next) {
		if (!strncasecmp(rptr->line, "Host: ", 6)) {
			if (strlen(WC->http_host) == 0) {
                        	safestrncpy(WC->http_host, &rptr->line[6],
					sizeof WC->http_host);
			}
                }
		if (!strncasecmp(rptr->line, "If-Match: ", 10)) {
                        safestrncpy(dav_ifmatch, &rptr->line[10],
				sizeof dav_ifmatch);
                }
		if (!strncasecmp(rptr->line, "Depth: ", 7)) {
			if (!strcasecmp(&rptr->line[7], "infinity")) {
				dav_depth = 32767;
			}
			else if (!strcmp(&rptr->line[7], "0")) {
				dav_depth = 0;
			}
			else if (!strcmp(&rptr->line[7], "1")) {
				dav_depth = 1;
			}
                }
	}

	if (!WC->logged_in) {
		wprintf("HTTP/1.1 401 Unauthorized\r\n");
		groupdav_common_headers();
		wprintf("WWW-Authenticate: Basic realm=\"%s\"\r\n",
			serv_info.serv_humannode);
		wprintf("Content-Length: 0\r\n\r\n");
		return;
	}

	extract_token(dav_method, req->line, 0, ' ', sizeof dav_method);
	extract_token(dav_pathname, req->line, 1, ' ', sizeof dav_pathname);
	unescape_input(dav_pathname);

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
	while (ds=strstr(dav_pathname, "//"), ds != NULL) {
		strcpy(ds, ds+1);
	}

	/*
	 * If there's an If-Match: header, strip out the quotes if present, and
	 * then if all that's left is an asterisk, make it go away entirely.
	 */
	if (strlen(dav_ifmatch) > 0) {
		striplt(dav_ifmatch);
		if (dav_ifmatch[0] == '\"') {
			strcpy(dav_ifmatch, &dav_ifmatch[1]);
			for (i=0; i<strlen(dav_ifmatch); ++i) {
				if (dav_ifmatch[i] == '\"') {
					dav_ifmatch[i] = 0;
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
	if (!strcasecmp(dav_method, "OPTIONS")) {
		groupdav_options(dav_pathname);
		return;
	}

	/*
	 * The PROPFIND method is basically used to list all objects in a
	 * room, or to list all relevant rooms on the server.
	 */
	if (!strcasecmp(dav_method, "PROPFIND")) {
		groupdav_propfind(dav_pathname, dav_depth,
				dav_content_type, dav_content);
		return;
	}

	/*
	 * The GET method is used for fetching individual items.
	 */
	if (!strcasecmp(dav_method, "GET")) {
		groupdav_get(dav_pathname);
		return;
	}

	/*
	 * The PUT method is used to add or modify items.
	 */
	if (!strcasecmp(dav_method, "PUT")) {
		groupdav_put(dav_pathname, dav_ifmatch,
				dav_content_type, dav_content,
				dav_content_length);
		return;
	}

	/*
	 * The DELETE method kills, maims, and destroys.
	 */
	if (!strcasecmp(dav_method, "DELETE")) {
		groupdav_delete(dav_pathname, dav_ifmatch);
		return;
	}

	/*
	 * Couldn't find what we were looking for.  Die in a car fire.
	 */
	wprintf("HTTP/1.1 501 Method not implemented\r\n");
	groupdav_common_headers();
	wprintf("Content-Type: text/plain\r\n"
		"\r\n"
		"GroupDAV method \"%s\" is not implemented.\r\n",
		dav_method
	);
}


/*
 * Output our host prefix for globally absolute URL's.
 */  
void groupdav_identify_host(void) {
	if (strlen(WC->http_host) > 0) {
		wprintf("%s://%s",
			(is_https ? "https" : "http"),
			WC->http_host);
	}
}
