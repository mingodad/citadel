/*
 * This is the main transaction loop of the web service.  It maintains a
 * persistent session to the Citadel server, handling HTTP WebCit requests as
 * they arrive and presenting a user interface.
 *
 * Copyright (c) 1996-2013 by the citadel.org team
 *
 * This program is open source software.  You can redistribute it and/or
 * modify it under the terms of the GNU General Public License, version 3.
 */

#define SHOW_ME_VAPPEND_PRINTF
#include <stdio.h>
#include <stdarg.h>
#include "webcit.h"
#include "dav.h"
#include "webserver.h"

StrBuf *csslocal = NULL;
HashList *HandlerHash = NULL;

void stuff_to_cookie(int unset_cookie);
extern int GetConnected(void);
extern int verbose;

void PutRequestLocalMem(void *Data, DeleteHashDataFunc DeleteIt)
{
        wcsession *WCC = WC;
	int n;
	
	n = GetCount(WCC->Hdr->HTTPHeaders);
	Put(WCC->Hdr->HTTPHeaders, IKEY(n), Data, DeleteIt);
}

void DeleteWebcitHandler(void *vHandler)
{
	WebcitHandler *Handler = (WebcitHandler*) vHandler;
	FreeStrBuf(&Handler->Name);
	FreeStrBuf(&Handler->DisplayName);
	free (Handler);
}

void WebcitAddUrlHandler(const char * UrlString, long UrlSLen, 
			 const char *DisplayName, long dslen,
			 WebcitHandlerFunc F, 
			 long Flags)
{
	WebcitHandler *NewHandler;	
	NewHandler = (WebcitHandler*) malloc(sizeof(WebcitHandler));
	NewHandler->F = F;
	NewHandler->Flags = Flags;
	NewHandler->Name = NewStrBufPlain(UrlString, UrlSLen);
	StrBufShrinkToFit(NewHandler->Name, 1);
	NewHandler->DisplayName = NewStrBufPlain(DisplayName, dslen);
	StrBufShrinkToFit(NewHandler->DisplayName, 1);
	Put(HandlerHash, UrlString, UrlSLen, NewHandler, DeleteWebcitHandler);
}

void tmplput_HANDLER_DISPLAYNAME(StrBuf *Target, WCTemplputParams *TP) 
{
	wcsession *WCC = WC;
	if (WCC->Hdr->HR.Handler != NULL)
		StrBufAppendTemplate(Target, TP, WCC->Hdr->HR.Handler->DisplayName, 0);
}


/*
 * web-printing funcion. uses our vsnprintf wrapper
 */
#ifdef UBER_VERBOSE_DEBUGGING
void wcc_printf(const char *FILE, const char *FUNCTION, long LINE, const char *format,...)
#else
void wc_printf(const char *format,...)
#endif
{
	wcsession *WCC = WC;
	va_list arg_ptr;

	if (WCC->WBuf == NULL)
		WCC->WBuf = NewStrBuf();
#ifdef UBER_VERBOSE_DEBUGGING
	StrBufAppendPrintf(WCC->WBuf, "\n%s:%s:%d[", FILE, FUNCTION, LINE);
#endif

	va_start(arg_ptr, format);
	StrBufVAppendPrintf(WCC->WBuf, format, arg_ptr);
	va_end(arg_ptr);
#ifdef UBER_VERBOSE_DEBUGGING
	StrBufAppendPrintf(WCC->WBuf, "]\n");
#endif
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
		wc_printf("</div> <!-- end of 'content' div -->\n");
		do_template("trailing");
	}

	/* If we've been saving it all up for one big output burst,
	 * go ahead and do that now.
	 */
	end_burst();
}


 

/*
 * Output HTTP headers and leading HTML for a page
 */
void output_headers(	int do_httpheaders,	/* 1 = output HTTP headers			  */
			int do_htmlhead,	/* 1 = output HTML <head> section and <body> opener */
			int do_room_banner,	/* 1 = include the room banner and <div id="content"></div> */
			int unset_cookies,	/* 1 = session is terminating, so unset the cookies */
			int suppress_check,	/* 1 = suppress check for instant messages	  */
			int cache		/* 1 = allow browser to cache this page	     */
) {
	wcsession *WCC = WC;
	char httpnow[128];

	if (WCC->isFailure) 
		hprintf("HTTP/2.2 500 Internal Server Error");
	else if (WCC->Hdr->HaveRange > 1)
		hprintf("HTTP/1.1 206 Partial Content\r\n");
	else
		hprintf("HTTP/1.1 200 OK\r\n");

	http_datestring(httpnow, sizeof httpnow, time(NULL));

	if (do_httpheaders) {
		if (WCC->serv_info != NULL)
			hprintf("Content-type: text/html; charset=utf-8\r\n"
				"Server: %s / %s\n"
				"Connection: close\r\n",
				PACKAGE_STRING, 
				ChrPtr(WCC->serv_info->serv_software));
		else
			hprintf("Content-type: text/html; charset=utf-8\r\n"
				"Server: %s / [n/a]\n"
				"Connection: close\r\n",
				PACKAGE_STRING);
	}

	if (cache > 0) {
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

	if (cache < 2) stuff_to_cookie(unset_cookies);

	if (do_htmlhead) {
		begin_burst();
		do_template("head");
		if ( (WCC->logged_in) && (!unset_cookies) ) {
			DoTemplate(HKEY("paging"), NULL, &NoCtx);
		}
		if (do_room_banner) {
			tmplput_roombanner(NULL, NULL);
		}
	}

	if (do_room_banner) {
		wc_printf("<div id=\"content\">\n");
	}
}

void output_custom_content_header(const char *ctype) {
	hprintf("HTTP/1.1 200 OK\r\n");
	hprintf("Content-type: %s; charset=utf-8\r\n",ctype);
	hprintf("Server: %s / %s\r\n", PACKAGE_STRING, ChrPtr(WC->serv_info->serv_software));
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
	stuff_to_cookie(0);
	begin_burst();
	wc_printf("<html><body>");
	wc_printf("Go <a href=\"%s\">here</A>.", whichpage);
	wc_printf("</body></html>\n");
	end_burst();
}



/*
 * Output a piece of content to the web browser using conformant HTTP and MIME semantics.
 *
 * If this function is called, it is expected that begin_burst() has already been called
 * and some sort of content has been fed into the buffer.  This function will transmit a
 * bunch of headers to the client.  end_burst() will add some headers of its own, and then
 * transmit the buffered content to the client.
 */
void http_transmit_thing(const char *content_type, int is_static)
{
	if (verbose)
		syslog(LOG_DEBUG, "http_transmit_thing(%s)%s", content_type, ((is_static > 0) ? " (static)" : ""));
	output_headers(0, 0, 0, 0, 0, is_static);

	hprintf("Content-type: %s\r\n"
		"Server: %s\r\n"
		"Connection: close\r\n",
		content_type,
		PACKAGE_STRING);

	end_burst();
}

void http_transmit_headers(const char *content_type, int is_static, long is_chunked, int is_gzip)
{
	wcsession *WCC = WC;
	if (verbose)
		syslog(LOG_DEBUG, "http_transmit_thing(%s)%s", content_type, ((is_static > 0) ? " (static)" : ""));
	output_headers(0, 0, 0, 0, 0, is_static);

	if (is_gzip)
		hprintf("Content-encoding: gzip\r\n");

	if (WCC->Hdr->HaveRange)
		hprintf("Accept-Ranges: bytes\r\n"
			"Content-Range: bytes %ld-%ld/%ld\r\n",
			WCC->Hdr->RangeStart,
			WCC->Hdr->RangeTil,
			WCC->Hdr->TotalBytes);

	hprintf("Content-type: %s\r\n"
		"Server: "PACKAGE_STRING"\r\n"
		"%s"
		"Connection: close\r\n\r\n",
		content_type,
		(is_chunked)?"Transfer-Encoding: chunked\r\n":"");
}


/*
 * Convenience functions to display a page containing only a string
 *
 * titlebarcolor	color of the titlebar of the frame
 * titlebarmsg		text to display in the title bar
 * messagetext		body of the box
 */
void convenience_page(const char *titlebarcolor, const char *titlebarmsg, const char *messagetext)
{
	hprintf("HTTP/1.1 200 OK\n");
	output_headers(1, 1, 1, 0, 0, 0);
	wc_printf("<div id=\"room_banner_override\">\n");
	wc_printf("<table width=100%% border=0 bgcolor=\"#%s\"><tr><td>", titlebarcolor);
	wc_printf("<span class=\"titlebar\">%s</span>\n", titlebarmsg);
	wc_printf("</td></tr></table>\n");
	wc_printf("</div>\n<div id=\"content\">\n");
	escputs(messagetext);
	wc_printf("<hr />\n");
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
	const StrBuf *MimeType;
	const StrBuf *Tmpl = sbstr("template");
	begin_burst();
	MimeType = DoTemplate(SKEY(Tmpl), NULL, &NoCtx);
	http_transmit_thing(ChrPtr(MimeType), 0);
}



/*
 * convenience function to indicate success
 */
void display_success(const char *successmessage)
{
	convenience_page("007700", "OK", successmessage);
}


/*
 * Authorization required page (sends a 401, causing the browser to request login credentials)
 */
void authorization_required(void)
{
	wcsession *WCC = WC;
	const char *message = "";

	hprintf("HTTP/1.1 401 Authorization Required\r\n");
	hprintf(
		"Server: %s / %s\r\n"
		"Connection: close\r\n",
		PACKAGE_STRING, ChrPtr(WC->serv_info->serv_software)
	);
	hprintf("WWW-Authenticate: Basic realm=\"%s\"\r\n", ChrPtr(WC->serv_info->serv_humannode));

	/* if this is a false cookie authentication, remove it to avoid endless loops. */
	if (StrLength(WCC->Hdr->HR.RawCookie) > 0)
		stuff_to_cookie(1);

	hprintf("Content-Type: text/html\r\n");
	begin_burst();
	wc_printf("<h1>");
	wc_printf(_("Authorization Required"));
	wc_printf("</h1>\r\n");

	if (WCC->ImportantMsg != NULL) {
		message = ChrPtr(WCC->ImportantMsg);
	}

	wc_printf(
		_("The resource you requested requires a valid username and password. "
		"You could not be logged in: %s\n"),
		message
	);
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
	wcsession *WCC = WC;
	int Done = 0;
	StrBuf *Buf;
	char *junk;
	size_t len;

	if (verbose)
		syslog(LOG_DEBUG, "ajax_servcmd() g_cmd=\"%s\"", bstr("g_cmd") );
	begin_ajax_response();
	Buf = NewStrBuf();
	serv_puts(bstr("g_cmd"));
	StrBuf_ServGetln(Buf);
	StrBufAppendBuf(WCC->WBuf, Buf, 0);
	StrBufAppendBufPlain(WCC->WBuf, HKEY("\n"), 0);
	
	switch (GetServerStatus(Buf, NULL)) {
	case 8:
		serv_puts("\n\n000");
		if ( (StrLength(Buf)==3) && 
		     !strcmp(ChrPtr(Buf), "000")) {
			StrBufAppendBufPlain(WCC->WBuf, HKEY("\000"), 0);
			break;
		}
	case 1:
		while (!Done) {
			if (StrBuf_ServGetln(Buf) < 0)
				break;
			if ( (StrLength(Buf)==3) && 
			     !strcmp(ChrPtr(Buf), "000")) {
				Done = 1;
			}
			StrBufAppendBuf(WCC->WBuf, Buf, 0);
			StrBufAppendBufPlain(WCC->WBuf, HKEY("\n"), 0);
		}
		break;
	case 4:
		text_to_server(bstr("g_input"));
		serv_puts("000");
		break;
	case 6:
		len = atol(&ChrPtr(Buf)[4]);
		StrBuf_ServGetBLOBBuffered(Buf, len);
		break;
	case 7:
		len = atol(&ChrPtr(Buf)[4]);
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
	 * that page_popup() doesn't try to open it a second time. TODO: page_popup isn't with us anymore.
	 */
	if (!strncasecmp(bstr("g_cmd"), "GEXP", 4)) {
		WCC->last_pager_check = time(NULL);
	}
	FreeStrBuf(&Buf);
}


/*
 * Helper function for the asynchronous check to see if we need
 * to open the instant messenger window.
 */
void seconds_since_last_gexp(void)
{
	char buf[256];

	if ( (time(NULL) - WC->last_pager_check) < 30) {
		wc_printf("NO\n");
	}
	else {
		memset(buf, 0, 5);
		serv_puts("NOOP");
		serv_getln(buf, sizeof buf);
		if (buf[3] == '*') {
			wc_printf("YES");
		}
		else {
			wc_printf("NO");
		}
	}
}


/*
 * Save a URL destination so we can go to it later
 */
void push_destination(void) {
	wcsession *WCC = WC;

	if (!WCC) {
		wc_printf("no session");
		return;
	}

	FreeStrBuf(&WCC->PushedDestination);
	WCC->PushedDestination = NewStrBufDup(sbstr("url"));
	if (verbose)
		syslog(LOG_DEBUG, "Push: %s", ChrPtr(WCC->PushedDestination));
	wc_printf("OK");
}


/*
 * Go to the URL saved by push_destination()
 */
void pop_destination(void) {
	wcsession *WCC = WC;

	/*
	 * If we are in the middle of a new user signup, the server may request that
	 * we first pass through a registration screen.
	 */
	if ((WCC) && (WCC->need_regi)) {
		if ((WCC->PushedDestination != NULL) && (StrLength(WCC->PushedDestination) > 0)) {
			/* Registering will take us to the My Citadel Config room, so save our place */
			StrBufAppendBufPlain(WCC->PushedDestination, HKEY("?go="), 0);
			StrBufUrlescAppend(WCC->PushedDestination, WCC->CurRoom.name, NULL);
		}
		WCC->need_regi = 0;
		display_reg(1);
		return;
	}

	/*
	 * Do something reasonable if we somehow ended up requesting a pop without
	 * having first done a push.
	 */
	if ( (!WCC) || (WCC->PushedDestination == NULL) || (StrLength(WCC->PushedDestination) == 0) ) {
		do_welcome();
		return;
	}

	/*
	 * All righty then!  We have a destination saved, so go there now.
	 */
	if (verbose)
		syslog(LOG_DEBUG, "Pop: %s", ChrPtr(WCC->PushedDestination));
	http_redirect(ChrPtr(WCC->PushedDestination));
}



int ReadPostData(void)
{
	int rc;
	int urlencoded_post = 0;
	wcsession *WCC = WC;
	StrBuf *content = NULL;
	
	urlencoded_post = (strncasecmp(ChrPtr(WCC->Hdr->HR.ContentType), "application/x-www-form-urlencoded", 33) == 0) ;

	content = NewStrBufPlain(NULL, WCC->Hdr->HR.ContentLength + 256);

	if (!urlencoded_post)
	{
		StrBufPrintf(content, 
		     "Content-type: %s\n"
			     "Content-length: %ld\n\n",
			     ChrPtr(WCC->Hdr->HR.ContentType), 
			     WCC->Hdr->HR.ContentLength);
	}

	/** Read the entire input data at once. */
	rc = client_read_to(WCC->Hdr, content, 
			    WCC->Hdr->HR.ContentLength,
			    SLEEPING);
	if (rc < 0)
		return rc;
		
	
	if (urlencoded_post) {
		ParseURLParams(content);
	} else if (!strncasecmp(ChrPtr(WCC->Hdr->HR.ContentType), "multipart", 9)) {
		char *Buf;
		char *BufEnd;
		long len;

		len = StrLength(content);
		Buf = SmashStrBuf(&content);
		BufEnd = Buf + len;
		mime_parser(Buf, BufEnd, *upload_handler, NULL, NULL, NULL, 0);
		free(Buf);
	} else if (WCC->Hdr->HR.ContentLength > 0) {
		WCC->upload = content;
		WCC->upload_length = StrLength(WCC->upload);
		content = NULL;
	}
	FreeStrBuf(&content);
	return 1;
}


int Conditional_REST_DEPTH(StrBuf *Target, WCTemplputParams *TP)
{
	long Depth, IsDepth;
	long offset = 0;
	wcsession *WCC = WC;

	if (WCC->Hdr->HR.Handler != NULL)
		offset ++;
	Depth = GetTemplateTokenNumber(Target, TP, 2, 0);
	IsDepth = GetCount(WCC->Directory) + offset;

//	LogTemplateError(Target, "bla", 1, TP, "REST_DEPTH: %ld : %ld\n", Depth, IsDepth);
	if (Depth < 0) {
		Depth = -Depth;
		return IsDepth > Depth;
	}
	else 
		return Depth == IsDepth;
}



/*
 * Entry point for WebCit transaction
 */
void session_loop(void)
{
	int xhttp;
	StrBuf *Buf;
	
	/*
	 * We stuff these with the values coming from the client cookies,
	 * so we can use them to reconnect a timed out session if we have to.
	 */
	wcsession *WCC;
      
	WCC= WC;
	WCC->upload_length = 0;
	WCC->upload = NULL;
	WCC->Hdr->nWildfireHeaders = 0;

	if (WCC->Hdr->HR.ContentLength > 0) {
		if (ReadPostData() < 0) {
			return;
		}
	}

	Buf = NewStrBuf();
	WCC->trailing_javascript = NewStrBuf();

	/* Convert base64-encoded URL's back to plain text */
	if (!strncmp(ChrPtr(WCC->Hdr->this_page), "/B64", 4)) {
		StrBufCutLeft(WCC->Hdr->this_page, 4);
		StrBufDecodeBase64(WCC->Hdr->this_page);
		http_redirect(ChrPtr(WCC->Hdr->this_page));
		goto SKIP_ALL_THIS_CRAP;
	}

	/* If there are variables in the URL, we must grab them now */
	if (WCC->Hdr->PlainArgs != NULL)
		ParseURLParams(WCC->Hdr->PlainArgs);

	/* If the client sent a nonce that is incorrect, kill the request. */
	if (havebstr("nonce")) {
		if (verbose)
			syslog(LOG_DEBUG, "Comparing supplied nonce %s to session nonce %d", 
			       bstr("nonce"), WCC->nonce
				);
		if (ibstr("nonce") != WCC->nonce) {
			syslog(LOG_INFO, "Ignoring request with mismatched nonce.");
			hprintf("HTTP/1.1 404 Security check failed\r\n");
			hprintf("Content-Type: text/plain\r\n");
			begin_burst();
			wc_printf("Security check failed.\r\n");
			end_burst();
			goto SKIP_ALL_THIS_CRAP;
		}
	}

	/*
	 * If we're not connected to a Citadel server, try to hook up the connection now.
	 */
	if (!WCC->connected) {
		if (GetConnected()) {
			hprintf("HTTP/1.1 503 Service Unavailable\r\n");
			hprintf("Content-Type: text/html\r\n");
			begin_burst();
			wc_printf("<html><head><title>503 Service Unavailable</title></head><body>\n");
			wc_printf(_("This program was unable to connect or stay "
				"connected to the Citadel server.  Please report "
				"this problem to your system administrator.")
			);
			wc_printf("<br>");
			wc_printf("<a href=\"http://www.citadel.org/doku.php/"
				"faq:generalquestions:webcit_unable_to_connect\">%s</a>",
				_("Read More...")
			);
			wc_printf("</body></html>\n");
			end_burst();
			goto SKIP_ALL_THIS_CRAP;
		}
	}

	/*
	 * If we're not logged in, but we have authentication data (either from
	 * a cookie or from http-auth), try logging in to Citadel using that.
	 */
	if (	(!WCC->logged_in)
		&& (StrLength(WCC->Hdr->c_username) > 0)
		&& (StrLength(WCC->Hdr->c_password) > 0)
	) {
		long Status;

		FlushStrBuf(Buf);
		serv_printf("USER %s", ChrPtr(WCC->Hdr->c_username));
		StrBuf_ServGetln(Buf);
		if (GetServerStatus(Buf, &Status) == 3) {
			serv_printf("PASS %s", ChrPtr(WCC->Hdr->c_password));
			StrBuf_ServGetln(Buf);
			if (GetServerStatus(Buf, NULL) == 2) {
				become_logged_in(WCC->Hdr->c_username,
						 WCC->Hdr->c_password, Buf);
			} else {
				/* Should only display when password is wrong */
				WCC->ImportantMsg = NewStrBufPlain(ChrPtr(Buf) + 4, StrLength(Buf) - 4);
				authorization_required();
				FreeStrBuf(&Buf);
				goto SKIP_ALL_THIS_CRAP;
			}
		}
		else if (Status == 541) {
			WCC->logged_in = 1;
		}
	}

	xhttp = (WCC->Hdr->HR.eReqType != eGET) &&
		(WCC->Hdr->HR.eReqType != ePOST) &&
		(WCC->Hdr->HR.eReqType != eHEAD);

	/*
	 * If a 'go' (or 'gotofirst') parameter has been specified, attempt to goto that room
	 * prior to doing anything else.
	 */
	if (havebstr("go")) {
		int ret;
		if (verbose)
			syslog(LOG_DEBUG, "Explicit room selection: %s", bstr("go"));
		ret = gotoroom(sbstr("go"));	/* do quietly to avoid session output! */
		if ((ret/100) != 2) {
			if (verbose)
				syslog(LOG_DEBUG, "Unable to change to [%s]; Reason: %d", bstr("go"), ret);
		}
	}
	else if (havebstr("gotofirst")) {
		int ret;
		if (verbose)
			syslog(LOG_DEBUG, "Explicit room selection: %s", bstr("gotofirst"));
		ret = gotoroom(sbstr("gotofirst"));	/* do quietly to avoid session output! */
		if ((ret/100) != 2) {
			syslog(LOG_INFO, "Unable to change to [%s]; Reason: %d", bstr("gotofirst"), ret);
		}
	}

	/*
	 * If we aren't in any room yet, but we have cookie data telling us where we're
	 * supposed to be, and 'go' was not specified, then go there.
	 */
	else if ( (StrLength(WCC->CurRoom.name) == 0) && ( (StrLength(WCC->Hdr->c_roomname) > 0) )) {
		int ret;

		if (verbose)
			syslog(LOG_DEBUG, "We are in '%s' but cookie indicates '%s', going there...",
			       ChrPtr(WCC->CurRoom.name),
			       ChrPtr(WCC->Hdr->c_roomname)
		);
		ret = gotoroom(WCC->Hdr->c_roomname);	/* do quietly to avoid session output! */
		if ((ret/100) != 2) {
			if (verbose)
				syslog(LOG_DEBUG, "COOKIEGOTO: Unable to change to [%s]; Reason: %d",
				       ChrPtr(WCC->Hdr->c_roomname), ret);
		}
	}

	if (WCC->Hdr->HR.Handler != NULL) {
		if (	(!WCC->logged_in)
			&& ((WCC->Hdr->HR.Handler->Flags & ANONYMOUS) == 0)
			&& (WCC->serv_info != NULL)
			&& (WCC->serv_info->serv_supports_guest == 0)
		) {
			display_login();
		}
		else {
			if ((WCC->Hdr->HR.Handler->Flags & AJAX) != 0) {
				begin_ajax_response();
			}
			WCC->Hdr->HR.Handler->F();
			if ((WCC->Hdr->HR.Handler->Flags & AJAX) != 0) {
				end_ajax_response();
			}
		}
	}
	/* When all else fails, display the default landing page or a main menu. */
	else {
		/* 
		 * ordinary browser users get a nice login screen, DAV etc. requsets
		 * are given a 401 so they can handle it appropriate.
		 */
		if (!WCC->logged_in)  {
			if (xhttp) {
				authorization_required();
			}
			else {
				display_default_landing_page();
			}
		}
		/*
		 * Toplevel dav requests? or just a flat browser request? 
		 */
		else {
			if (xhttp) {
				dav_main();
			}
			else {
				display_main_menu();
			}
		}
	}

SKIP_ALL_THIS_CRAP:
	FreeStrBuf(&Buf);
	fflush(stdout);
}



/*
 * Display the appropriate landing page for this site.
 */
void display_default_landing_page(void) {
	wcsession *WCC = WC;

	if (WCC && WCC->serv_info && WCC->serv_info->serv_supports_guest) {
		/* default action */

		if (havebstr("go")) {
			if (verbose)
				syslog(LOG_DEBUG, "Explicit room selection: %s", bstr("go"));
			smart_goto(sbstr("go"));
		}
		else if (default_landing_page) {
			http_redirect(default_landing_page);
		}
		else {
			StrBuf *teh_lobby = NewStrBufPlain(HKEY("_BASEROOM_"));
			smart_goto(teh_lobby);
			FreeStrBuf(&teh_lobby);
		}
	}
	else {
		display_login();
	}
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

int Conditional_IS_HTTPS(StrBuf *Target, WCTemplputParams *TP)
{
	return is_https != 0;
}

void AppendImportantMessage(const char *pch, long len)
{
	wcsession *WCC = WC;

	if (StrLength(WCC->ImportantMsg) > 0) {
		StrBufAppendBufPlain(WCC->ImportantMsg, HKEY("\n"), 0);
	}
		
	StrBufAppendBufPlain(WCC->ImportantMsg, pch, len, 0);
}

int ConditionalImportantMesage(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;
	if (WCC != NULL)
		return (StrLength(WCC->ImportantMsg) > 0);
	else
		return 0;
}

void tmplput_importantmessage(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;
	
	if (WCC != NULL) {
		if (StrLength(WCC->ImportantMsg) > 0) {
			StrEscAppend(Target, WCC->ImportantMsg, NULL, 0, 0);
		}
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
	StrBufAppendBuf(Target, 
			csslocal, 0);
}

void tmplput_packagestring(StrBuf *Target, WCTemplputParams *TP)
{
	StrBufAppendBufPlain(Target, 
			     HKEY(PACKAGE_STRING), 0);
}

extern char static_local_dir[PATH_MAX];


void 
InitModule_WEBCIT
(void)
{
	char dir[SIZ];
	WebcitAddUrlHandler(HKEY("blank"), "", 0, blank_page, ANONYMOUS|COOKIEUNNEEDED|ISSTATIC);
	WebcitAddUrlHandler(HKEY("landing"), "", 0, display_default_landing_page, ANONYMOUS|COOKIEUNNEEDED);
	WebcitAddUrlHandler(HKEY("do_template"), "", 0, url_do_template, ANONYMOUS);
	WebcitAddUrlHandler(HKEY("sslg"), "", 0, seconds_since_last_gexp, AJAX|LOGCHATTY);
	WebcitAddUrlHandler(HKEY("ajax_servcmd"), "", 0, ajax_servcmd, 0);
	WebcitAddUrlHandler(HKEY("webcit"), "", 0, blank_page, URLNAMESPACE);
	WebcitAddUrlHandler(HKEY("push"), "", 0, push_destination, AJAX);
	WebcitAddUrlHandler(HKEY("pop"), "", 0, pop_destination, 0);

	WebcitAddUrlHandler(HKEY("401"), "", 0, authorization_required, ANONYMOUS|COOKIEUNNEEDED);
	RegisterConditional("COND:IMPMSG", 0, ConditionalImportantMesage, CTX_NONE);
	RegisterConditional("COND:REST:DEPTH", 0, Conditional_REST_DEPTH, CTX_NONE);
	RegisterConditional("COND:IS_HTTPS", 0, Conditional_IS_HTTPS, CTX_NONE);

	RegisterNamespace("CSSLOCAL", 0, 0, tmplput_csslocal, NULL, CTX_NONE);
	RegisterNamespace("IMPORTANTMESSAGE", 0, 0, tmplput_importantmessage, NULL, CTX_NONE);
	RegisterNamespace("TRAILING_JAVASCRIPT", 0, 0, tmplput_trailing_javascript, NULL, CTX_NONE);
	RegisterNamespace("URL:DISPLAYNAME", 0, 1, tmplput_HANDLER_DISPLAYNAME, NULL, CTX_NONE);
	RegisterNamespace("PACKAGESTRING", 0, 1, tmplput_packagestring, NULL, CTX_NONE);

	
	snprintf(dir, SIZ, "%s/webcit.css", static_local_dir);
	if (!access(dir, R_OK)) {
		syslog(LOG_INFO, "Using local Stylesheet [%s]", dir);
		csslocal = NewStrBufPlain(HKEY("<link href=\"static.local/webcit.css\" rel=\"stylesheet\" type=\"text/css\" />"));
	}
	else
		syslog(LOG_INFO, "No Site-local Stylesheet [%s] installed.", dir);

}

void
ServerStartModule_WEBCIT
(void)
{
	HandlerHash = NewHash(1, NULL);
}


void 
ServerShutdownModule_WEBCIT
(void)
{
	FreeStrBuf(&csslocal);
	DeleteHash(&HandlerHash);
}



void
SessionNewModule_WEBCIT
(wcsession *sess)
{
	sess->ImportantMsg = NewStrBuf();
	sess->WBuf = NewStrBufPlain(NULL, SIZ * 4);
	sess->HBuf = NewStrBufPlain(NULL, SIZ / 4);
}

void
SessionDetachModule_WEBCIT
(wcsession *sess)
{
	DeleteHash(&sess->Directory);

	FreeStrBuf(&sess->upload);
	sess->upload_length = 0;
	
	FreeStrBuf(&sess->trailing_javascript);

	if (StrLength(sess->WBuf) > SIZ * 30) /* Bigger than 120K? release. */
	{
		FreeStrBuf(&sess->WBuf);
		sess->WBuf = NewStrBuf();
	}
	else
		FlushStrBuf(sess->WBuf);
	FlushStrBuf(sess->HBuf);
	if (StrLength(sess->ImportantMsg) > 0) {
		FlushStrBuf(sess->ImportantMsg);
	}

}

void 
SessionDestroyModule_WEBCIT
(wcsession *sess)
{
	FreeStrBuf(&sess->WBuf);
	FreeStrBuf(&sess->HBuf);
	FreeStrBuf(&sess->ImportantMsg);
	FreeStrBuf(&sess->PushedDestination);
}

