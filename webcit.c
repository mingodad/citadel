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
StrBuf *csslocal = NULL;
HashList *HandlerHash = NULL;


void DeleteWebcitHandler(void *vHandler)
{
	WebcitHandler *Handler = (WebcitHandler*) vHandler;
	FreeStrBuf(&Handler->Name);
	free (Handler);

}

void WebcitAddUrlHandler(const char * UrlString, 
			 long UrlSLen, 
			 WebcitHandlerFunc F, 
			 long Flags)
{
	WebcitHandler *NewHandler;	
	NewHandler = (WebcitHandler*) malloc(sizeof(WebcitHandler));
	NewHandler->F = F;
	NewHandler->Flags = Flags;
	NewHandler->Name = NewStrBufPlain(UrlString, UrlSLen);
	StrBufShrinkToFit(NewHandler->Name, 1);
	Put(HandlerHash, UrlString, UrlSLen, NewHandler, DeleteWebcitHandler);
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
	wcsession *WCC = WC;
	char cookie[1024];
	char httpnow[128];

	hprintf("HTTP/1.1 200 OK\n");
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

	if (cache < 2) {

		stuff_to_cookie(cookie, 1024, 
				WCC->wc_session,
				WCC->wc_username,
				WCC->wc_password,
				WCC->wc_roomname,
				get_selected_language()
			);
		
		if (unset_cookies) {
			hprintf("Set-cookie: webcit=%s; path=/\r\n", unset);
		} else {
			hprintf("Set-cookie: webcit=%s; path=/\r\n", cookie);
			if (server_cookie != NULL) {
				hprintf("%s\n", server_cookie);
			}
		}
	}

	if (do_htmlhead) {
		begin_burst();
		do_template("head", NULL);

		/* check for ImportantMessages (these display in a div overlaying the main screen) */
		if (!IsEmptyStr(WCC->ImportantMessage)) {
			wprintf("<div id=\"important_message\">\n"
				"<span class=\"imsg\">");
			StrEscAppend(WCC->WBuf, NULL, WCC->ImportantMessage, 0, 0);
			wprintf("</span><br />\n"
				"</div>\n"
			);
			StrBufAppendBufPlain(WCC->trailing_javascript,
					     HKEY("setTimeout('hide_imsg_popup()', 5000);	\n"), 
					     0
			);
			WCC->ImportantMessage[0] = 0;
		}
		else if (StrLength(WCC->ImportantMsg) > 0) {
			wprintf("<div id=\"important_message\">\n"
				"<span class=\"imsg\">");
			StrEscAppend(WCC->WBuf, WCC->ImportantMsg, NULL, 0, 0);
			wprintf("</span><br />\n"
				"</div>\n"
			);
			StrBufAppendBufPlain(WCC->trailing_javascript,
					     HKEY("setTimeout('hide_imsg_popup()', 5000);	\n"),
					     0
			);
			FlushStrBuf(WCC->ImportantMsg);
		}
		if ( (WCC->logged_in) && (!unset_cookies) ) {
			/*DoTemplate(HKEY("iconbar"), NULL, &NoCtx);*/
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
		((is_static > 0) ? " (static)" : "")
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
 * Convenience functions to display a page containing only a string
 *
 * titlebarcolor	color of the titlebar of the frame
 * titlebarmsg		text to display in the title bar
 * messagetext		body of the box
 */
void convenience_page(const char *titlebarcolor, const char *titlebarmsg, const char *messagetext)
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
	const StrBuf *MimeType;
	const StrBuf *Tmpl = sbstr("template");
	begin_burst();
	MimeType = DoTemplate(SKEY(Tmpl), NULL, &NoCtx);
	http_transmit_thing(ChrPtr(MimeType), 0);
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
	hprintf("Content-Type: text/html\r\n");
	wprintf("<h1>");
	wprintf(_("Authorization Required"));
	wprintf("</h1>\r\n");
	

	if (WCC->ImportantMsg != NULL)
		message = ChrPtr(WCC->ImportantMsg);
	else if (WCC->ImportantMessage != NULL)
		message = WCC->ImportantMessage;

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
	wcsession *WCC = WC;
	int Done = 0;
	StrBuf *Buf;
	char *junk;
	size_t len;

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
			StrBuf_ServGetln(Buf);
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
	 * that page_popup() doesn't try to open it a second time.
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



void ReadPostData(void)
{
	const char *content_end = NULL;
	int body_start = 0;
	wcsession *WCC = WC;
	StrBuf *content = NULL;
	
	content = NewStrBufPlain(NULL, WCC->Hdr->HR.ContentLength + 256);

	StrBufPrintf(content, 
		     "Content-type: %s\n"
		     "Content-length: %ld\n\n",
		     ChrPtr(WCC->Hdr->HR.ContentType), 
			     WCC->Hdr->HR.ContentLength);
/*
  hprintf("Content-type: %s\n"
  "Content-length: %d\n\n",
  ContentType, ContentLength);
*/
	body_start = StrLength(content);

	/** Read the entire input data at once. */
	client_read_to(WCC->Hdr, content, 
		       WCC->Hdr->HR.ContentLength,
		       SLEEPING);
	
	if (!strncasecmp(ChrPtr(WCC->Hdr->HR.ContentType), "application/x-www-form-urlencoded", 33)) {
		StrBufCutLeft(content, body_start);
		ParseURLParams(content);
	} else if (!strncasecmp(ChrPtr(WCC->Hdr->HR.ContentType), "multipart", 9)) {
		content_end = ChrPtr(content) + 
			WCC->Hdr->HR.ContentLength + 
			body_start;
		mime_parser(ChrPtr(content), content_end, *upload_handler, NULL, NULL, NULL, 0);
	}
	FreeStrBuf(&content);
}


/*
 * Entry point for WebCit transaction
 */
void session_loop(void)
{
	int Flags = 0;
	int xhttp;
	StrBuf *Buf;
	
	/*
	 * We stuff these with the values coming from the client cookies,
	 * so we can use them to reconnect a timed out session if we have to.
	 */
	wcsession *WCC;

	
	Buf = NewStrBuf();

	WCC= WC;

	WCC->upload_length = 0;
	WCC->upload = NULL;
	WCC->is_mobile = 0;
	WCC->trailing_javascript = NewStrBuf();
	WCC->Hdr->nWildfireHeaders = 0;
	if (WCC->Hdr->HR.Handler != NULL)
		Flags = WCC->Hdr->HR.Handler->Flags; /* so we can temporarily add our own... */

	if (WCC->Hdr->HR.ContentLength > 0) {
		ReadPostData();
	}

	/* If there are variables in the URL, we must grab them now */
	if (WCC->Hdr->PlainArgs != NULL)
		ParseURLParams(WCC->Hdr->PlainArgs);

	/* If the client sent a nonce that is incorrect, kill the request. */
	if (havebstr("nonce")) {
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
		if (GetConnected ())
			goto SKIP_ALL_THIS_CRAP;
	}


	/*
	 * If we're not logged in, but we have authentication data (either from
	 * a cookie or from http-auth), try logging in to Citadel using that.
	 */
	if ((!WCC->logged_in)
	    && (StrLength(WCC->Hdr->c_username) > 0)
	    && (StrLength(WCC->Hdr->c_password) > 0))
	{
		FlushStrBuf(Buf);
		serv_printf("USER %s", ChrPtr(WCC->Hdr->c_username));
		StrBuf_ServGetln(Buf);
		if (GetServerStatus(Buf, NULL) == 3) {
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
	}

	xhttp = (WCC->Hdr->HR.eReqType != eGET) &&
		(WCC->Hdr->HR.eReqType != ePOST) &&
		(WCC->Hdr->HR.eReqType != eHEAD);

	/*
	 * If a 'gotofirst' parameter has been specified, attempt to goto that room
	 * prior to doing anything else.
	 */
	if (havebstr("gotofirst")) {
		int ret;
		ret = gotoroom(sbstr("gotofirst"));	/* do quietly to avoid session output! */
		if ((ret/100) != 2) {
			lprintf(1, "GOTOFIRST: Unable to change to [%s]; Reason: %d\n",
				bstr("gotofirst"), ret);
		}
	}

	/*
	 * If we aren't in any room yet, but we have cookie data telling us where we're
	 * supposed to be, and 'gotofirst' was not specified, then go there.
	 */
	else if ( (StrLength(WCC->wc_roomname) == 0) && ( (StrLength(WCC->Hdr->c_roomname) > 0) )) {
		int ret;

		lprintf(9, "We are in '%s' but cookie indicates '%s', going there...\n",
			ChrPtr(WCC->wc_roomname),
			ChrPtr(WCC->Hdr->c_roomname)
		);
		ret = gotoroom(WCC->Hdr->c_roomname);	/* do quietly to avoid session output! */
		if ((ret/100) != 2) {
			lprintf(1, "COOKIEGOTO: Unable to change to [%s]; Reason: %d\n",
				ChrPtr(WCC->Hdr->c_roomname), ret);
		}
	}

	if (WCC->Hdr->HR.Handler != NULL) {
		if (!WCC->logged_in && ((WCC->Hdr->HR.Handler->Flags & ANONYMOUS) == 0)) {
			display_login(NULL);
		}
		else {
			if ((WCC->Hdr->HR.Handler->Flags & AJAX) != 0)
				begin_ajax_response();
			WCC->Hdr->HR.Handler->F();
			if ((WCC->Hdr->HR.Handler->Flags & AJAX) != 0)
				end_ajax_response();
		}
	}
	/* When all else fais, display the main menu. */
	else {
		/* 
		 * ordinary browser users get a nice login screen, DAV etc. requsets
		 * are given a 401 so they can handle it appropriate.
		 */
		if (!WCC->logged_in)  {
			if (xhttp)
				authorization_required();
			else 
				display_login(NULL);
		}
		/*
		 * Toplevel dav requests? or just a flat browser request? 
		 */
		else {
			if (xhttp)
				groupdav_main();
			else
				display_main_menu();
		}
	}

SKIP_ALL_THIS_CRAP:
	FreeStrBuf(&Buf);
	fflush(stdout);
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
		return ((!IsEmptyStr(WCC->ImportantMessage)) || 
			(StrLength(WCC->ImportantMsg) > 0));
	else
		return 0;
}

void tmplput_importantmessage(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;
	
	if (WCC != NULL) {
		if (!IsEmptyStr(WCC->ImportantMessage)) {
			StrEscAppend(Target, NULL, WCC->ImportantMessage, 0, 0);
			WCC->ImportantMessage[0] = '\0';
		}
		else if (StrLength(WCC->ImportantMsg) > 0) {
			StrEscAppend(Target, WCC->ImportantMsg, NULL, 0, 0);
			FlushStrBuf(WCC->ImportantMsg);
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

extern char static_local_dir[PATH_MAX];


void 
InitModule_WEBCIT
(void)
{
	char dir[SIZ];
	WebcitAddUrlHandler(HKEY("blank"), blank_page, ANONYMOUS|COOKIEUNNEEDED|ISSTATIC);
	WebcitAddUrlHandler(HKEY("do_template"), url_do_template, ANONYMOUS);
	WebcitAddUrlHandler(HKEY("sslg"), seconds_since_last_gexp, AJAX|LOGCHATTY);
	WebcitAddUrlHandler(HKEY("ajax_servcmd"), ajax_servcmd, 0);
	WebcitAddUrlHandler(HKEY("webcit"), blank_page, URLNAMESPACE);

	WebcitAddUrlHandler(HKEY("401"), authorization_required, ANONYMOUS|COOKIEUNNEEDED);
	RegisterConditional(HKEY("COND:IMPMSG"), 0, ConditionalImportantMesage, CTX_NONE);
	RegisterNamespace("CSSLOCAL", 0, 0, tmplput_csslocal, CTX_NONE);
	RegisterNamespace("IMPORTANTMESSAGE", 0, 0, tmplput_importantmessage, CTX_NONE);
	RegisterNamespace("TRAILING_JAVASCRIPT", 0, 0, tmplput_trailing_javascript, CTX_NONE);

	snprintf(dir, SIZ, "%s/static.local/webcit.css", static_local_dir);
	if (!access(dir, R_OK)) {
		lprintf(9, "Using local Stylesheet [%s]\n", dir);
		csslocal = NewStrBufPlain(HKEY("<link href=\"static.local/webcit.css\" rel=\"stylesheet\" type=\"text/css\">"));
	}
	else
		lprintf(9, "Didn't find site local Stylesheet [%s]\n", dir);

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
	DeleteHash(&sess->Hdr->urlstrings);// TODO?
	if (sess->upload_length > 0) {
		free(sess->upload);
		sess->upload_length = 0;
	}
	FreeStrBuf(&sess->trailing_javascript);

	if (StrLength(sess->WBuf) > SIZ * 30) /* Bigger than 120K? release. */
	{
		FreeStrBuf(&sess->WBuf);
		sess->WBuf = NewStrBuf();
	}
	else
		FlushStrBuf(sess->WBuf);
	FlushStrBuf(sess->HBuf);
}

void 
SessionDestroyModule_WEBCIT
(wcsession *sess)
{
	FreeStrBuf(&sess->WBuf);
	FreeStrBuf(&sess->HBuf);
	FreeStrBuf(&sess->ImportantMsg);
}

