/*
 * $Id$
 *
 * This is the main transaction loop of the web service.  It maintains a
 * persistent session to the Citadel server, handling HTTP WebCit requests as
 * they arrive and presenting a user interface.
 */
#define SHOW_ME_VAPPEND_PRINTF
#include <stdio.h>
#include <stdarg.h>
#include "webcit.h"
#include "groupdav.h"
#include "webserver.h"


/*
 * String to unset the cookie.
 * Any date "in the past" will work, so I chose my birthday, right down to
 * the exact minute.  :)
 */
static char *unset = "; expires=28-May-1971 18:10:00 GMT";

HashList *HandlerHash = NULL;

void WebcitAddUrlHandler(const char * UrlString, 
			 long UrlSLen, 
			 WebcitHandlerFunc F, 
			 long Flags)
{
	WebcitHandler *NewHandler;

	if (HandlerHash == NULL)
		HandlerHash = NewHash(1, NULL);
	
	NewHandler = (WebcitHandler*) malloc(sizeof(WebcitHandler));
	NewHandler->F = F;
	NewHandler->Flags = Flags;
	Put(HandlerHash, UrlString, UrlSLen, NewHandler, NULL);
}


/*
 * web-printing funcion. uses our vsnprintf wrapper
 */
void wprintf(const char *format,...)
{
	wcsession *WCC = WC;
	va_list arg_ptr;

	if (WCC->WBuf == NULL)
		WCC->WBuf = NewStrBuf();

	va_start(arg_ptr, format);
	StrBufVAppendPrintf(WCC->WBuf, format, arg_ptr);
	va_end(arg_ptr);
}

/*
 * http-header-printing funcion. uses our vsnprintf wrapper
 */
void hprintf(const char *format,...)
{
	wcsession *WCC = WC;
	va_list arg_ptr;

	va_start(arg_ptr, format);
	StrBufVAppendPrintf(WCC->HBuf, format, arg_ptr);
	va_end(arg_ptr);
}



/*
 * wrap up an HTTP session, closes tags, etc.
 *
 * print_standard_html_footer should be set to:
 * 0		- to transmit only,
 * nonzero	- to append the closing tags
 */
void wDumpContent(int print_standard_html_footer)
{
	if (print_standard_html_footer) {
		wprintf("</div> <!-- end of 'content' div -->\n");
		do_template("trailing", NULL);
	}

	/* If we've been saving it all up for one big output burst,
	 * go ahead and do that now.
	 */
	end_burst();
}


 

/*
 * Output HTTP headers and leading HTML for a page
 */
void output_headers(	int do_httpheaders,	/* 1 = output HTTP headers                          */
			int do_htmlhead,	/* 1 = output HTML <head> section and <body> opener */

			int do_room_banner,	/* 0=no, 1=yes,                                     
						 * 2 = I'm going to embed my own, so don't open the 
						 *     <div id="content"> either.                   
						 */

			int unset_cookies,	/* 1 = session is terminating, so unset the cookies */
			int suppress_check,	/* 1 = suppress check for instant messages          */
			int cache		/* 1 = allow browser to cache this page             */
) {
	char cookie[1024];
	char httpnow[128];

	hprintf("HTTP/1.1 200 OK\n");
	http_datestring(httpnow, sizeof httpnow, time(NULL));

	if (do_httpheaders) {
		hprintf("Content-type: text/html; charset=utf-8\r\n"
			"Server: %s / %s\n"
			"Connection: close\r\n",
			PACKAGE_STRING, 
			ChrPtr(serv_info.serv_software)
		);
	}

	if (cache) {
		char httpTomorow[128];

		http_datestring(httpTomorow, sizeof httpTomorow, 
				time(NULL) + 60 * 60 * 24 * 2);

		hprintf("Pragma: public\r\n"
			"Cache-Control: max-age=3600, must-revalidate\r\n"
			"Last-modified: %s\r\n"
			"Expires: %s\r\n",
			httpnow,
			httpTomorow
		);
	}
	else {
		hprintf("Pragma: no-cache\r\n"
			"Cache-Control: no-store\r\n"
			"Expires: -1\r\n"
		);
	}

	stuff_to_cookie(cookie, 1024, 
			WC->wc_session, WC->wc_username,
			WC->wc_password, WC->wc_roomname);

	if (unset_cookies) {
		hprintf("Set-cookie: webcit=%s; path=/\r\n", unset);
	} else {
		hprintf("Set-cookie: webcit=%s; path=/\r\n", cookie);
		if (server_cookie != NULL) {
			hprintf("%s\n", server_cookie);
		}
	}

	if (do_htmlhead) {
		begin_burst();
		do_template("head", NULL);

		/* check for ImportantMessages (these display in a div overlaying the main screen) */
		if (!IsEmptyStr(WC->ImportantMessage)) {
			wprintf("<div id=\"important_message\">\n"
				"<span class=\"imsg\">");
			escputs(WC->ImportantMessage);
			wprintf("</span><br />\n"
				"</div>\n"
				"<script type=\"text/javascript\">\n"
				"	 setTimeout('hide_imsg_popup()', 5000);	\n"
				"</script>\n");
			WC->ImportantMessage[0] = 0;
		}

		if ( (WC->logged_in) && (!unset_cookies) ) {
			DoTemplate(HKEY("iconbar"), NULL, &NoCtx);
			page_popup();
		}

		if (do_room_banner == 1) {
			wprintf("<div id=\"banner\">\n");
			embed_room_banner(NULL, navbar_default);
			wprintf("</div>\n");
		}
	}

	if (do_room_banner == 1) {
		wprintf("<div id=\"content\">\n");
	}
}

void output_custom_content_header(const char *ctype) {
  hprintf("HTTP/1.1 200 OK\r\n");
  hprintf("Content-type: %s; charset=utf-8\r\n",ctype);
  hprintf("Server: %s / %s\r\n", PACKAGE_STRING, ChrPtr(serv_info.serv_software));
  hprintf("Connection: close\r\n");
}


/*
 * Generic function to do an HTTP redirect.  Easy and fun.
 */
void http_redirect(const char *whichpage) {
	hprintf("HTTP/1.1 302 Moved Temporarily\n");
	hprintf("Location: %s\r\n", whichpage);
	hprintf("URI: %s\r\n", whichpage);
	hprintf("Content-type: text/html; charset=utf-8\r\n");
	wprintf("<html><body>");
	wprintf("Go <a href=\"%s\">here</A>.", whichpage);
	wprintf("</body></html>\n");
	end_burst();
}



/*
 * Output a piece of content to the web browser using conformant HTTP and MIME semantics
 */
void http_transmit_thing(const char *content_type,
			 int is_static) {

#ifndef TECH_PREVIEW
	lprintf(9, "http_transmit_thing(%s)%s\n",
		content_type,
		(is_static ? " (static)" : "")
	);
#endif
	output_headers(0, 0, 0, 0, 0, is_static);

	hprintf("Content-type: %s\r\n"
		"Server: %s\r\n"
		"Connection: close\r\n",
		content_type,
		PACKAGE_STRING);

	end_burst();
}

/*
 * print menu box like used in the floor view or admin interface.
 * This function takes pair of strings as va_args, 
 * Title	Title string of the box
 * Class	CSS Class for the box
 * nLines	How many string pairs should we print? (URL, UrlText)
 * ...		Pairs of URL Strings and their Names
 */
void print_menu_box(char* Title, char *Class, int nLines, ...)
{
	va_list arg_list;
	long i;
	
	svput("BOXTITLE", WCS_STRING, Title);
	do_template("beginboxx", NULL);
	
	wprintf("<ul class=\"%s\">", Class);
	
	va_start(arg_list, nLines);
	for (i = 0; i < nLines; ++i)
	{ 
		wprintf("<li><a href=\"%s\">", va_arg(arg_list, char *));
		wprintf((char *) va_arg(arg_list, char *));
		wprintf("</a></li>\n");
	}
	va_end (arg_list);
	
	wprintf("</a></li>\n");
	
	wprintf("</ul>");
	
	do_template("endbox", NULL);
}


/*
 * dump out static pages from disk
 */
void output_static(char *what)
{
	int fd;
	struct stat statbuf;
	off_t bytes;
	off_t count = 0;
	const char *content_type;
	int len;
	const char *Err;

	fd = open(what, O_RDONLY);
	if (fd <= 0) {
		lprintf(9, "output_static('%s')  -- NOT FOUND --\n", what);
		hprintf("HTTP/1.1 404 %s\r\n", strerror(errno));
		hprintf("Content-Type: text/plain\r\n");
		wprintf("Cannot open %s: %s\r\n", what, strerror(errno));
		end_burst();
	} else {
		len = strlen (what);
		content_type = GuessMimeByFilename(what, len);

		if (fstat(fd, &statbuf) == -1) {
			lprintf(9, "output_static('%s')  -- FSTAT FAILED --\n", what);
			hprintf("HTTP/1.1 404 %s\r\n", strerror(errno));
			hprintf("Content-Type: text/plain\r\n");
			wprintf("Cannot fstat %s: %s\n", what, strerror(errno));
			end_burst();
			return;
		}

		count = 0;
		bytes = statbuf.st_size;

		if (StrBufReadBLOB(WC->WBuf, &fd, 1, bytes, &Err) < 0)
		{
			if (fd > 0) close(fd);
			lprintf(9, "output_static('%s')  -- FREAD FAILED (%s) --\n", what, strerror(errno));
				hprintf("HTTP/1.1 500 internal server error \r\n");
				hprintf("Content-Type: text/plain\r\n");
				end_burst();
				return;
		}


		close(fd);
#ifndef TECH_PREVIEW
		lprintf(9, "output_static('%s')  %s\n", what, content_type);
#endif
		http_transmit_thing(content_type, 1);
	}
	if (yesbstr("force_close_session")) {
		end_webcit_session();
	}
}


/*
 * Convenience functions to display a page containing only a string
 *
 * titlebarcolor	color of the titlebar of the frame
 * titlebarmsg		text to display in the title bar
 * messagetext		body of the box
 */
void convenience_page(char *titlebarcolor, char *titlebarmsg, char *messagetext)
{
	hprintf("HTTP/1.1 200 OK\n");
	output_headers(1, 1, 2, 0, 0, 0);
	wprintf("<div id=\"banner\">\n");
	wprintf("<table width=100%% border=0 bgcolor=\"#%s\"><tr><td>", titlebarcolor);
	wprintf("<span class=\"titlebar\">%s</span>\n", titlebarmsg);
	wprintf("</td></tr></table>\n");
	wprintf("</div>\n<div id=\"content\">\n");
	escputs(messagetext);

	wprintf("<hr />\n");
	wDumpContent(1);
}


/*
 * Display a blank page.
 */
void blank_page(void) {
	output_headers(1, 1, 0, 0, 0, 0);
	wDumpContent(2);
}


/*
 * A template has been requested
 */
void url_do_template(void) {
	const StrBuf *Tmpl = sbstr("template");
	begin_burst();
	output_headers(1, 0, 0, 0, 1, 0);
	DoTemplate(SKEY(Tmpl), NULL, &NoCtx);
	end_burst();
}



/*
 * convenience function to indicate success
 */
void display_success(char *successmessage)
{
	convenience_page("007700", "OK", successmessage);
}


/*
 * Authorization required page 
 * This is probably temporary and should be revisited 
 */
void authorization_required(const char *message)
{
	hprintf("HTTP/1.1 401 Authorization Required\r\n");
	hprintf("WWW-Authenticate: Basic realm=\"%s\"\r\n", ChrPtr(serv_info.serv_humannode));
	hprintf("Content-Type: text/html\r\n");
	wprintf("<h1>");
	wprintf(_("Authorization Required"));
	wprintf("</h1>\r\n");
	wprintf(_("The resource you requested requires a valid username and password. "
		"You could not be logged in: %s\n"), message);
	wDumpContent(0);
	
}

/*
 * Convenience functions to wrap around asynchronous ajax responses
 */
void begin_ajax_response(void) {
	wcsession *WCC = WC;

	FlushStrBuf(WCC->HBuf);
        output_headers(0, 0, 0, 0, 0, 0);

        hprintf("Content-type: text/html; charset=UTF-8\r\n"
                "Server: %s\r\n"
                "Connection: close\r\n"
		,
                PACKAGE_STRING);
        begin_burst();
}

/*
 * print ajax response footer 
 */
void end_ajax_response(void) {
        wDumpContent(0);
}

/*
 * Wraps a Citadel server command in an AJAX transaction.
 */
void ajax_servcmd(void)
{
	char buf[1024];
	char gcontent[1024];
	char *junk;
	size_t len;

	begin_ajax_response();

	serv_printf("%s", bstr("g_cmd"));
	serv_getln(buf, sizeof buf);
	wprintf("%s\n", buf);

	if (buf[0] == '8') {
		serv_printf("\n\n000");
	}
	if ((buf[0] == '1') || (buf[0] == '8')) {
		while (serv_getln(gcontent, sizeof gcontent), strcmp(gcontent, "000")) {
			wprintf("%s\n", gcontent);
		}
		wprintf("000");
	}
	if (buf[0] == '4') {
		text_to_server(bstr("g_input"));
		serv_puts("000");
	}
	if (buf[0] == '6') {
		len = atol(&buf[4]);
		junk = malloc(len);
		serv_read(junk, len);
		free(junk);
	}
	if (buf[0] == '7') {
		len = atol(&buf[4]);
		junk = malloc(len);
		memset(junk, 0, len);
		serv_write(junk, len);
		free(junk);
	}

	end_ajax_response();
	
	/*
	 * This is kind of an ugly hack, but this is the only place it can go.
	 * If the command was GEXP, then the instant messenger window must be
	 * running, so reset the "last_pager_check" watchdog timer so
	 * that page_popup() doesn't try to open it a second time.
	 */
	if (!strncasecmp(bstr("g_cmd"), "GEXP", 4)) {
		WC->last_pager_check = time(NULL);
	}
}


/*
 * Helper function for the asynchronous check to see if we need
 * to open the instant messenger window.
 */
void seconds_since_last_gexp(void)
{
	char buf[256];

	if ( (time(NULL) - WC->last_pager_check) < 30) {
		wprintf("NO\n");
	}
	else {
		memset(buf, 0, 5);
		serv_puts("NOOP");
		serv_getln(buf, sizeof buf);
		if (buf[3] == '*') {
			wprintf("YES");
		}
		else {
			wprintf("NO");
		}
	}
}

/**
 * \brief Detects a 'mobile' user agent 
 */
int is_mobile_ua(char *user_agent) {
      if (strstr(user_agent,"iPhone OS") != NULL) {
	return 1;
      } else if (strstr(user_agent,"Windows CE") != NULL) {
	return 1;
      } else if (strstr(user_agent,"SymbianOS") != NULL) {
	return 1;
      } else if (strstr(user_agent, "Opera Mobi") != NULL) {
	return 1;
      } else if (strstr(user_agent, "Firefox/2.0.0 Opera 9.51 Beta") != NULL) {
	      /*  For some reason a new install of Opera 9.51beta decided to spoof. */
	  return 1;
	  }
      return 0;
}


/*
 * Entry point for WebCit transaction
 */
void session_loop(HashList *HTTPHeaders, StrBuf *ReqLine, StrBuf *request_method, StrBuf *ReadBuf)
{
	StrBuf *Buf;
	const char *pch, *pchs, *pche;
	void *vLine;
	char action[1024];
	char arg[8][128];
	size_t sizes[10];
	char *index[10];
	char buf[SIZ];
	int a, nBackDots, nEmpty;
	int ContentLength = 0;
	StrBuf *ContentType = NULL;
	StrBuf *UrlLine = NULL;
	StrBuf *content = NULL;
	const char *content_end = NULL;
	StrBuf *browser_host = NULL;
	char user_agent[256];
	int body_start = 0;
	int is_static = 0;
	int n_static = 0;
	int len = 0;
	void *vHandler;
	WebcitHandler *Handler;

	/*
	 * We stuff these with the values coming from the client cookies,
	 * so we can use them to reconnect a timed out session if we have to.
	 */
	StrBuf *c_username;
	StrBuf *c_password;
	StrBuf *c_roomname;
	char c_httpauth_string[SIZ];
	StrBuf *c_httpauth_user;
	StrBuf *c_httpauth_pass;
	wcsession *WCC;
	
	Buf = NewStrBuf();
	c_username = NewStrBuf();
	c_password = NewStrBuf();
	c_roomname = NewStrBuf();
	safestrncpy(c_httpauth_string, "", sizeof c_httpauth_string);
	c_httpauth_user = NewStrBufPlain(HKEY(DEFAULT_HTTPAUTH_USER));
	c_httpauth_pass = NewStrBufPlain(HKEY(DEFAULT_HTTPAUTH_PASS));

	WCC= WC;
	if (WCC->WBuf == NULL)
		WC->WBuf = NewStrBufPlain(NULL, 32768);
	FlushStrBuf(WCC->WBuf);

	if (WCC->HBuf == NULL)
		WCC->HBuf = NewStrBuf();
	FlushStrBuf(WCC->HBuf);

	WCC->upload_length = 0;
	WCC->upload = NULL;
	WCC->is_mobile = 0;
	WCC->trailing_javascript = NewStrBuf();

	/** Figure out the action */
	index[0] = action;
	sizes[0] = sizeof action;
	for (a=1; a<9; a++)
	{
		index[a] = arg[a-1];
		sizes[a] = sizeof arg[a-1];
	}
/*///	index[9] = &foo; todo */
	nBackDots = 0;
	nEmpty = 0;
	for ( a = 0; a < 9; ++a)
	{
		extract_token(index[a], ChrPtr(ReqLine), a + 1, '/', sizes[a]);
		if (strstr(index[a], "?")) *strstr(index[a], "?") = 0;
		if (strstr(index[a], "&")) *strstr(index[a], "&") = 0;
		if (strstr(index[a], " ")) *strstr(index[a], " ") = 0;
		if ((index[a][0] == '.') && (index[a][1] == '.'))
			nBackDots++;
		if (index[a][0] == '\0')
			nEmpty++;
	}


	if (GetHash(HTTPHeaders, HKEY("COOKIE"), &vLine) && 
	    (vLine != NULL)){
		cookie_to_stuff((StrBuf *)vLine, NULL,
				c_username,
				c_password,
				c_roomname);
	}
	if (GetHash(HTTPHeaders, HKEY("AUTHORIZATION"), &vLine) &&
	    (vLine!=NULL)) {
/* TODO: wrap base64 in strbuf */
		CtdlDecodeBase64(c_httpauth_string, ChrPtr((StrBuf*)vLine), StrLength((StrBuf*)vLine));
		FlushStrBuf(Buf);
		StrBufAppendBufPlain(Buf, c_httpauth_string, -1, 0);
		StrBufExtract_token(c_httpauth_user, Buf, 0, ':');
		StrBufExtract_token(c_httpauth_pass, Buf, 1, ':');
	}
	if (GetHash(HTTPHeaders, HKEY("CONTENT-LENGTH"), &vLine) &&
	    (vLine!=NULL)) {
		ContentLength = StrToi((StrBuf*)vLine);
	}
	if (GetHash(HTTPHeaders, HKEY("CONTENT-TYPE"), &vLine) &&
	    (vLine!=NULL)) {
		ContentType = (StrBuf*)vLine;
	}
	if (GetHash(HTTPHeaders, HKEY("USER-AGENT"), &vLine) &&
	    (vLine!=NULL)) {
		safestrncpy(user_agent, ChrPtr((StrBuf*)vLine), sizeof user_agent);
#ifdef TECH_PREVIEW
		if ((WCC->is_mobile < 0) && is_mobile_ua(&buf[12])) {			
			WCC->is_mobile = 1;
		}
		else {
			WCC->is_mobile = 0;
		}
#endif
	}
	if ((follow_xff) &&
	    GetHash(HTTPHeaders, HKEY("X-FORWARDED-HOST"), &vLine) &&
	    (vLine != NULL)) {
		WCC->http_host = (StrBuf*)vLine;
	}
	if ((StrLength(WCC->http_host) == 0) && 
	    GetHash(HTTPHeaders, HKEY("HOST"), &vLine) &&
	    (vLine!=NULL)) {
		WCC->http_host = (StrBuf*)vLine;
	}

	if (GetHash(HTTPHeaders, HKEY("X-FORWARDED-FOR"), &vLine) &&
	    (vLine!=NULL)) {
		browser_host = (StrBuf*) vLine;

		while (StrBufNum_tokens(browser_host, ',') > 1) {
			StrBufRemove_token(browser_host, 0, ',');
		}
		StrBufTrim(browser_host);
	}

	if (ContentLength > 0) {
		content = NewStrBuf();
		StrBufPrintf(content, "Content-type: %s\n"
			 "Content-length: %d\n\n",
			 ChrPtr(ContentType), ContentLength);
/*
		hprintf("Content-type: %s\n"
			"Content-length: %d\n\n",
			ContentType, ContentLength);
*/
		body_start = StrLength(content);

		/** Read the entire input data at once. */
		client_read(&WCC->http_sock, content, ReadBuf, ContentLength + body_start);

		if (!strncasecmp(ChrPtr(ContentType), "application/x-www-form-urlencoded", 33)) {
			StrBufCutLeft(content, body_start);
			ParseURLParams(content);
		} else if (!strncasecmp(ChrPtr(ContentType), "multipart", 9)) {
			content_end = ChrPtr(content) + ContentLength + body_start;
			mime_parser(ChrPtr(content), content_end, *upload_handler, NULL, NULL, NULL, 0);
		}
	} else {
		content = NULL;
	}

	/* make a note of where we are in case the user wants to save it */
	WCC->this_page = NewStrBufDup(ReqLine);
	StrBufRemove_token(WCC->this_page, 2, ' ');
	StrBufRemove_token(WCC->this_page, 0, ' ');

	/* If there are variables in the URL, we must grab them now */
	UrlLine = NewStrBufDup(ReqLine);
	len = StrLength(UrlLine);
	pch = pchs = ChrPtr(UrlLine);
	pche = pchs + len;
	while (pch < pche) {
		if ((*pch == '?') || (*pch == '&')) {
			StrBufCutLeft(UrlLine, pch - pchs + 1);
			ParseURLParams(UrlLine);
			break;
		}
		pch ++;
	}
	FreeStrBuf(&UrlLine);

	/* If it's a "force 404" situation then display the error and bail. */
	if (!strcmp(action, "404")) {
		hprintf("HTTP/1.1 404 Not found\r\n");
		hprintf("Content-Type: text/plain\r\n");
		wprintf("Not found\r\n");
		end_burst();
		goto SKIP_ALL_THIS_CRAP;
	}

	/* Static content can be sent without connecting to Citadel. */
	is_static = 0;
	for (a=0; a<ndirs && ! is_static; ++a) {
		if (!strcasecmp(action, (char*)static_content_dirs[a])) { /* map web to disk location */
			is_static = 1;
			n_static = a;
		}
	}
	if (is_static) {
		if (nBackDots < 2)
		{
			snprintf(buf, sizeof buf, "%s/%s/%s/%s/%s/%s/%s/%s",
				 static_dirs[n_static], 
				 index[1], index[2], index[3], index[4], index[5], index[6], index[7]);
			for (a=0; a<8; ++a) {
				if (buf[strlen(buf)-1] == '/') {
					buf[strlen(buf)-1] = 0;
				}
			}
			for (a = 0; a < strlen(buf); ++a) {
				if (isspace(buf[a])) {
					buf[a] = 0;
				}
			}
			output_static(buf);
		}
		else 
		{
			lprintf(9, "Suspicious request. Ignoring.");
			hprintf("HTTP/1.1 404 Security check failed\r\n");
			hprintf("Content-Type: text/plain\r\n\r\n");
			wprintf("You have sent a malformed or invalid request.\r\n");
			end_burst();
		}
		goto SKIP_ALL_THIS_CRAP;	/* Don't try to connect */
	}

	/* If the client sent a nonce that is incorrect, kill the request. */
	if (strlen(bstr("nonce")) > 0) {
		lprintf(9, "Comparing supplied nonce %s to session nonce %ld\n", 
			bstr("nonce"), WCC->nonce);
		if (ibstr("nonce") != WCC->nonce) {
			lprintf(9, "Ignoring request with mismatched nonce.\n");
			hprintf("HTTP/1.1 404 Security check failed\r\n");
			hprintf("Content-Type: text/plain\r\n\r\n");
			wprintf("Security check failed.\r\n");
			end_burst();
			goto SKIP_ALL_THIS_CRAP;
		}
	}

	/*
	 * If we're not connected to a Citadel server, try to hook up the
	 * connection now.
	 */
	if (!WCC->connected) {
		if (!strcasecmp(ctdlhost, "uds")) {
			/* unix domain socket */
			snprintf(buf, SIZ, "%s/citadel.socket", ctdlport);
			WCC->serv_sock = uds_connectsock(buf);
		}
		else {
			/* tcp socket */
			WCC->serv_sock = tcp_connectsock(ctdlhost, ctdlport);
		}

		if (WCC->serv_sock < 0) {
			do_logout();
			goto SKIP_ALL_THIS_CRAP;
		}
		else {
			WCC->connected = 1;
			serv_getln(buf, sizeof buf);	/* get the server greeting */

			/* Are there too many users already logged in? */
			if (!strncmp(buf, "571", 3)) {
				wprintf(_("This server is already serving its maximum number of users and cannot accept any additional logins at this time.  Please try again later or contact your system administrator."));
				end_burst();
				end_webcit_session();
				goto SKIP_ALL_THIS_CRAP;
			}

			/*
			 * From what host is our user connecting?  Go with
			 * the host at the other end of the HTTP socket,
			 * unless we are following X-Forwarded-For: headers
			 * and such a header has already turned up something.
			 */
			if ( (!follow_xff) || (StrLength(browser_host) == 0) ) {
				if (browser_host == NULL) {
					browser_host = NewStrBuf();
					Put(HTTPHeaders, HKEY("FreeMeWithTheOtherHeaders"), 
					    browser_host, HFreeStrBuf);
				}
				locate_host(browser_host, WCC->http_sock);
			}

			get_serv_info(browser_host, user_agent);
			if (serv_info.serv_rev_level < MINIMUM_CIT_VERSION) {
				begin_burst();
				wprintf(_("You are connected to a Citadel "
					"server running Citadel %d.%02d. \n"
					"In order to run this version of WebCit "
					"you must also have Citadel %d.%02d or"
					" newer.\n\n\n"),
						serv_info.serv_rev_level / 100,
						serv_info.serv_rev_level % 100,
						MINIMUM_CIT_VERSION / 100,
						MINIMUM_CIT_VERSION % 100
					);
				end_burst();
				end_webcit_session();
				goto SKIP_ALL_THIS_CRAP;
			}
		}
	}
/*///////todo: restore language in this case */
	/*
	 * Functions which can be performed without logging in
	 */
	if (!strcasecmp(action, "listsub")) {
		do_listsub();
		goto SKIP_ALL_THIS_CRAP;
	}
	if (!strcasecmp(action, "freebusy")) {
		do_freebusy(ChrPtr(ReqLine));
		goto SKIP_ALL_THIS_CRAP;
	}

	/*
	 * If we're not logged in, but we have HTTP Authentication data,
	 * try logging in to Citadel using that.
	 */
	if ((!WCC->logged_in)
	    && (StrLength(c_httpauth_user) > 0)
	    && (StrLength(c_httpauth_pass) > 0))
	{
		FlushStrBuf(Buf);
		serv_printf("USER %s", ChrPtr(c_httpauth_user));
		StrBuf_ServGetln(Buf);
		if (GetServerStatus(Buf, NULL) == 3) {
			serv_printf("PASS %s", ChrPtr(c_httpauth_pass));
			StrBuf_ServGetln(Buf);
			if (GetServerStatus(Buf, NULL) == 2) {
				become_logged_in(c_httpauth_user,
						c_httpauth_pass, Buf);
				if (WCC->httpauth_user == NULL)
					WCC->httpauth_user = NewStrBufDup(c_httpauth_user);
				else {
					FlushStrBuf(WCC->httpauth_user);
					StrBufAppendBuf(WCC->httpauth_user, c_httpauth_user, 0);
				}
				if (WCC->httpauth_pass == NULL)
					WCC->httpauth_pass = NewStrBufDup(c_httpauth_pass);
				else {
					FlushStrBuf(WCC->httpauth_pass);
					StrBufAppendBuf(WCC->httpauth_pass, c_httpauth_pass, 0);
				}
			} else {
				/* Should only display when password is wrong */
				authorization_required(&buf[4]);
				FreeStrBuf(&Buf);
				goto SKIP_ALL_THIS_CRAP;
			}
		}
	}

	/* This needs to run early */
#ifdef TECH_PREVIEW
	if (!strcasecmp(action, "rss")) {
		display_rss(sbstr("room"), request_method);
		goto SKIP_ALL_THIS_CRAP;
	}
#endif

	/* 
	 * The GroupDAV stuff relies on HTTP authentication instead of
	 * our session's authentication.
	 */
	if (!strncasecmp(action, "groupdav", 8)) {
		groupdav_main(HTTPHeaders, 
			      ReqLine, request_method,
			      ContentType, /* do GroupDAV methods */
			      ContentLength, content, body_start);
		if (!WCC->logged_in) {
			WCC->killthis = 1;	/* If not logged in, don't */
		}				/* keep the session active */
		goto SKIP_ALL_THIS_CRAP;
	}


	/*
	 * Automatically send requests with any method other than GET or
	 * POST to the GroupDAV code as well.
	 */
	if ((strcasecmp(ChrPtr(request_method), "GET")) && (strcasecmp(ChrPtr(request_method), "POST"))) {
		groupdav_main(HTTPHeaders, ReqLine, 
			      request_method, ContentType, /** do GroupDAV methods */
			      ContentLength, content, body_start);
		if (!WCC->logged_in) {
			WCC->killthis = 1;	/** If not logged in, don't */
		}				/** keep the session active */
		goto SKIP_ALL_THIS_CRAP;
	}

	/*
	 * If we're not logged in, but we have username and password cookies
	 * supplied by the browser, try using them to log in.
	 */
	if ((!WCC->logged_in)
	   && (StrLength(c_username)>0)
	   && (StrLength(c_password)>0)) {
		serv_printf("USER %s", ChrPtr(c_username));
		StrBuf_ServGetln(Buf);
		if (GetServerStatus(Buf, NULL) == 3) {
			serv_printf("PASS %s", ChrPtr(c_password));
			StrBuf_ServGetln(Buf);
			if (GetServerStatus(Buf, NULL) == 2) {
				StrBuf *Lang;
				become_logged_in(c_username, c_password, Buf);
				if (get_preference("language", &Lang)) {
					set_selected_language(ChrPtr(Lang));
					go_selected_language();		/* set locale */
				}
				get_preference("default_header_charset", &WCC->DefaultCharset);
			}
		}
	}

	/*
	 * If a 'gotofirst' parameter has been specified, attempt to goto that room
	 * prior to doing anything else.
	 */
	if (havebstr("gotofirst")) {
		gotoroom(sbstr("gotofirst"));	/* do this quietly to avoid session output! */
	}

	/*
	 * If we don't have a current room, but a cookie specifying the
	 * current room is supplied, make an effort to go there.
	 */
	if ((StrLength(WCC->wc_roomname) == 0) && (StrLength(c_roomname) > 0)) {
		serv_printf("GOTO %s", ChrPtr(c_roomname));
		StrBuf_ServGetln(Buf);
		if (GetServerStatus(Buf, NULL) == 2) {
			if (WCC->wc_roomname == NULL) {
				WCC->wc_roomname = NewStrBufDup(c_roomname);
			}
			else {
				FlushStrBuf(WCC->wc_roomname);
				StrBufAppendBuf(WCC->wc_roomname, c_roomname, 0);
			}
		}
	}
	
	GetHash(HandlerHash, action, strlen(action) /* TODO*/, &vHandler),
		Handler = (WebcitHandler*) vHandler;
	if (Handler != NULL) {
		if (!WCC->logged_in && ((Handler->Flags & ANONYMOUS) == 0)) {
			display_login(NULL);
		}
		else {
			if((Handler->Flags & NEED_URL)) {
				if (WCC->UrlFragment1 == NULL)
					WCC->UrlFragment1 = NewStrBuf();
				if (WCC->UrlFragment2 == NULL)
					WCC->UrlFragment2 = NewStrBuf();
				if (WCC->UrlFragment3 == NULL)
					WCC->UrlFragment3 = NewStrBuf();
				StrBufPrintf(WCC->UrlFragment1, "%s", index[0]);
				StrBufPrintf(WCC->UrlFragment2, "%s", index[1]);
				StrBufPrintf(WCC->UrlFragment3, "%s", index[2]);
			}
			if ((Handler->Flags & AJAX) != 0)
				begin_ajax_response();
			Handler->F();
			if ((Handler->Flags & AJAX) != 0)
				end_ajax_response();
		}
	}
	/* When all else fais, display the main menu. */
	else {
		if (!WCC->logged_in) 
			display_login(NULL);
		else
			display_main_menu();
	}

SKIP_ALL_THIS_CRAP:
	if (WCC->SavePrefsToServer) {
		save_preferences();
		WCC->SavePrefsToServer = 0;
	}
	FreeStrBuf(&Buf);
	FreeStrBuf(&c_username);
	FreeStrBuf(&c_password);
	FreeStrBuf(&c_roomname);
	FreeStrBuf(&c_httpauth_user);
	FreeStrBuf(&c_httpauth_pass);
	FreeStrBuf(&WCC->this_page);
	fflush(stdout);
	if (content != NULL) {
		FreeStrBuf(&content);
		content = NULL;
	}
	DeleteHash(&WCC->urlstrings);
	if (WCC->upload_length > 0) {
		free(WCC->upload);
		WCC->upload_length = 0;
	}
	FreeStrBuf(&WCC->trailing_javascript);
}


/*
 * Replacement for sleep() that uses select() in order to avoid SIGALRM
 */
void sleeeeeeeeeep(int seconds)
{
	struct timeval tv;

	tv.tv_sec = seconds;
	tv.tv_usec = 0;
	select(0, NULL, NULL, NULL, &tv);
}


int ConditionalImportantMesage(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;
	if (WCC != NULL)
		return (!IsEmptyStr(WCC->ImportantMessage));
	else
		return 0;
}

void tmplput_importantmessage(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;
	
	if (WCC != NULL) {
/*
		StrBufAppendTemplate(Target, nArgs, Tokens, Context, ContextType,
				     WCC->ImportantMessage, 0);
*/
		StrEscAppend(Target, NULL, WCC->ImportantMessage, 0, 0);
		WCC->ImportantMessage[0] = '\0';
	}
}

void tmplput_trailing_javascript(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;

	if (WCC != NULL)
		StrBufAppendTemplate(Target, TP, WCC->trailing_javascript, 0);
}

void tmplput_csslocal(StrBuf *Target, WCTemplputParams *TP)
{
	extern StrBuf *csslocal;
	StrBufAppendBuf(Target, 
			csslocal, 0);
}




void 
InitModule_WEBCIT
(void)
{
	WebcitAddUrlHandler(HKEY("blank"), blank_page, ANONYMOUS);
	WebcitAddUrlHandler(HKEY("do_template"), url_do_template, ANONYMOUS);
	WebcitAddUrlHandler(HKEY("sslg"), seconds_since_last_gexp, AJAX);
	WebcitAddUrlHandler(HKEY("ajax_servcmd"), ajax_servcmd, 0);

	RegisterConditional(HKEY("COND:IMPMSG"), 0, ConditionalImportantMesage, CTX_NONE);
	RegisterNamespace("CSSLOCAL", 0, 0, tmplput_csslocal, CTX_NONE);
	RegisterNamespace("IMPORTANTMESSAGE", 0, 0, tmplput_importantmessage, CTX_NONE);
	RegisterNamespace("TRAILING_JAVASCRIPT", 0, 0, tmplput_trailing_javascript, CTX_NONE);
}
