/*
 * webcit.c
 *
 * This is the actual program called by the webserver.  It maintains a
 * persistent session to the Citadel server, handling HTTP WebCit requests as
 * they arrive and presenting a user interface.
 *
 * $Id$
 */

#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <limits.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <stdarg.h>
#include <pthread.h>
#include <signal.h>
#include "webcit.h"
#include "webserver.h"
#include "mime_parser.h"

/*
 * String to unset the cookie.
 * Any date "in the past" will work, so I chose my birthday, right down to
 * the exact minute.  :)
 */
static char *unset = "; expires=28-May-1971 18:10:00 GMT";

void unescape_input(char *buf)
{
	int a, b;
	char hex[3];

	while ((isspace(buf[strlen(buf) - 1])) && (strlen(buf) > 0))
		buf[strlen(buf) - 1] = 0;

	for (a = 0; a < strlen(buf); ++a) {
		if (buf[a] == '+')
			buf[a] = ' ';
		if (buf[a] == '%') {
			hex[0] = buf[a + 1];
			hex[1] = buf[a + 2];
			hex[2] = 0;
			sscanf(hex, "%02x", &b);
			buf[a] = (char) b;
			strcpy(&buf[a + 1], &buf[a + 3]);
		}
	}

}


void addurls(char *url)
{
	char *up, *ptr;
	char buf[SIZ];
	int a, b;
	struct urlcontent *u;

	up = url;
	while (strlen(up) > 0) {

		/* locate the = sign */
		safestrncpy(buf, up, sizeof buf);
		b = (-1);
		for (a = 255; a >= 0; --a)
			if (buf[a] == '=')
				b = a;
		if (b < 0)
			return;
		buf[b] = 0;

		u = (struct urlcontent *) malloc(sizeof(struct urlcontent));
		u->next = WC->urlstrings;
		WC->urlstrings = u;
		strcpy(u->url_key, buf);

		/* now chop that part off */
		for (a = 0; a <= b; ++a)
			++up;

		/* locate "&" and "?" delimiters */
		ptr = up;
		b = strlen(up);
		for (a = 0; a < strlen(up); ++a) {
			if ( (ptr[0] == '&') || (ptr[0] == '?') ) {
				b = a;
				break;
			}
			++ptr;
		}
		ptr = up;
		for (a = 0; a < b; ++a)
			++ptr;
		strcpy(ptr, "");

		u->url_data = malloc(strlen(up) + 2);
		strcpy(u->url_data, up);
		u->url_data[b] = 0;
		unescape_input(u->url_data);
		up = ptr;
		++up;
	}
}

void free_urls(void)
{
	struct urlcontent *u;

	while (WC->urlstrings != NULL) {
		free(WC->urlstrings->url_data);
		u = WC->urlstrings->next;
		free(WC->urlstrings);
		WC->urlstrings = u;
	}
}

/*
 * Diagnostic function to display the contents of all variables
 */
void dump_vars(void)
{
	struct urlcontent *u;

	for (u = WC->urlstrings; u != NULL; u = u->next) {
		wprintf("%38s = %s\n", u->url_key, u->url_data);
	}
}

char *bstr(char *key)
{
	struct urlcontent *u;

	for (u = WC->urlstrings; u != NULL; u = u->next) {
		if (!strcasecmp(u->url_key, key))
			return (u->url_data);
	}
	return ("");
}


void wprintf(const char *format,...)
{
	va_list arg_ptr;
	char wbuf[4096];

	va_start(arg_ptr, format);
	vsnprintf(wbuf, sizeof wbuf, format, arg_ptr);
	va_end(arg_ptr);

	client_write(wbuf, strlen(wbuf));
}


/*
 * wDumpContent() wraps up an HTTP session, closes tags, etc.
 *
 * print_standard_html_footer should be set to 0 to transmit only, 1 to
 * append the main menu and closing tags, or 2 to
 * append the closing tags only.
 */
void wDumpContent(int print_standard_html_footer)
{
	if (print_standard_html_footer) {
		do_template("trailing");
	}


}


/*
 * Copy a string, escaping characters which have meaning in HTML.  If
 * nbsp is nonzero, spaces are converted to non-breaking spaces.
 */
void stresc(char *target, char *strbuf, int nbsp, int nolinebreaks)
{
	int a;
	strcpy(target, "");

	for (a = 0; a < strlen(strbuf); ++a) {
		if (strbuf[a] == '<')
			strcat(target, "&lt;");
		else if (strbuf[a] == '>')
			strcat(target, "&gt;");
		else if (strbuf[a] == '&')
			strcat(target, "&amp;");
		else if (strbuf[a] == '\"')
			strcat(target, "&quot;");
		else if (strbuf[a] == '\'') 
			strcat(target, "&#39;");
		else if (strbuf[a] == LB)
			strcat(target, "<");
		else if (strbuf[a] == RB)
			strcat(target, ">");
		else if (strbuf[a] == QU)
			strcat(target, "\"");
		else if ((strbuf[a] == 32) && (nbsp == 1))
			strcat(target, "&nbsp;");
		else if ((strbuf[a] == '\n') && (nolinebreaks))
			strcat(target, "");	/* nothing */
		else if ((strbuf[a] == '\r') && (nolinebreaks))
			strcat(target, "");	/* nothing */
		else
			strncat(target, &strbuf[a], 1);
	}
}

void escputs1(char *strbuf, int nbsp, int nolinebreaks)
{
	char *buf;

	if (strbuf == NULL) return;
	buf = malloc( (3 * strlen(strbuf)) + SIZ );
	stresc(buf, strbuf, nbsp, nolinebreaks);
	wprintf("%s", buf);
	free(buf);
}

void escputs(char *strbuf)
{
	escputs1(strbuf, 0, 0);
}

/*
 * Escape a string for feeding out as a URL.
 * Returns a pointer to a buffer that must be freed by the caller!
 */
void urlesc(char *outbuf, char *strbuf)
{
	int a, b, c;
	char *ec = " #&;`'|*?-~<>^()[]{}$\\";

	strcpy(outbuf, "");

	for (a = 0; a < strlen(strbuf); ++a) {
		c = 0;
		for (b = 0; b < strlen(ec); ++b) {
			if (strbuf[a] == ec[b])
				c = 1;
		}
		b = strlen(outbuf);
		if (c == 1)
			sprintf(&outbuf[b], "%%%02x", strbuf[a]);
		else
			sprintf(&outbuf[b], "%c", strbuf[a]);
	}
}

void urlescputs(char *strbuf)
{
	char outbuf[SIZ];
	
	urlesc(outbuf, strbuf);
	wprintf("%s", outbuf);
}


/*
 * Copy a string, escaping characters for JavaScript strings.
 */
void jsesc(char *target, char *strbuf)
{
	int a;
	strcpy(target, "");

	for (a = 0; a < strlen(strbuf); ++a) {
		if (strbuf[a] == '<')
			strcat(target, "[");
		else if (strbuf[a] == '>')
			strcat(target, "]");
		else if (strbuf[a] == '\"')
			strcat(target, "&quot;");
		else if (strbuf[a] == '&')
			strcat(target, "&amp;;");
		else if (strbuf[a] == '\'') 
			strcat(target, "\\'");
		else {
			strncat(target, &strbuf[a], 1);
		}
	}
}

void jsescputs(char *strbuf)
{
	char outbuf[SIZ];
	
	jsesc(outbuf, strbuf);
	wprintf("%s", outbuf);
}

/*
 * Copy a string, escaping characters for message text hold
 */
void msgesc(char *target, char *strbuf)
{
	int a;
	strcpy(target, "");

	for (a = 0; a < strlen(strbuf); ++a) {
		if (strbuf[a] == '\'') 
			strcat(target, "\\'");
		else if (strbuf[a] == '\n')
			strcat(target, " ");
		else if (strbuf[a] == '\r')
			strcat(target, " ");
		else {
			strncat(target, &strbuf[a], 1);
		}
	}
}

void msgescputs(char *strbuf) {
	char *outbuf;

	if (strbuf == NULL) return;
	outbuf = malloc( (3 * strlen(strbuf)) + SIZ);
	msgesc(outbuf, strbuf);
	wprintf("%s", outbuf);
	free(outbuf);
}




/*
 * Output all that important stuff that the browser will want to see
 *
 * control codes:
 * 
 * Bits 0 and 1:
 * 0 = Nothing.  Do not display any leading HTTP or HTML.
 * 1 = HTTP headers plus the room banner
 * 2 = HTTP headers required to terminate the session (unset cookies)
 * 3 = HTTP and HTML headers, but no room banner
 *
 * Bit 2: Set to 1 to auto-refresh page every 30 seconds
 * Bit 3: suppress check for express messages
 * Bit 4: Allow browser to cache this document
 *
 */
void output_headers(int controlcode)
{
	char cookie[SIZ];
	int print_standard_html_head = 0;
	int refresh30 = 0;
	int suppress_check = 0;
	int cache = 0;
	char httpnow[SIZ];
	char onload_fcn[SIZ];
	static int pageseq = 0;
	print_standard_html_head	=	controlcode & 0x03;
	refresh30			=	((controlcode & 0x04) >> 2);
	suppress_check			=	((controlcode & 0x08) >> 3);
	cache				=	((controlcode & 0x10) >> 4);

	wprintf("HTTP/1.0 200 OK\n");

	httpdate(httpnow, time(NULL));

	if (print_standard_html_head > 0) {
		wprintf("Content-type: text/html\n"
			"Server: %s\n", SERVER
		);
		if (!cache)
			wprintf("Connection: close\n"
				"Pragma: no-cache\n"
				"Cache-Control: no-store\n"
			);
	}

	stuff_to_cookie(cookie, WC->wc_session, WC->wc_username,
			WC->wc_password, WC->wc_roomname);

	if (print_standard_html_head == 2) {
		wprintf("Set-cookie: webcit=%s; path=/\n", unset);
	} else {
		wprintf("Set-cookie: webcit=%s; path=/\n", cookie);
		if (server_cookie != NULL) {
			wprintf("%s\n", server_cookie);
		}
	}

	if (print_standard_html_head > 0) {
		wprintf("\n");

		if (refresh30) svprintf("REFRESHTAG", WCS_STRING,
			"<META HTTP-EQUIV=\"refresh\" CONTENT=\"30\">\n");
		else svprintf("REFRESHTAG", WCS_STRING,
			"<META HTTP-EQUIV=\"refresh\" CONTENT=\"500363689;\">\n");
		/* script for checking for pages (not always launched) */

		sprintf(onload_fcn, "function onload_fcn() { \n");
		if (!WC->outside_frameset_allowed) {
			strcat(onload_fcn, "  force_frameset();  \n");
		}
		if (!suppress_check) if (WC->HaveExpressMessages) {
			strcat(onload_fcn, "  launch_page_popup();  \n");
			WC->HaveExpressMessages = 0;
		}
		strcat(onload_fcn, "} \n");

		svprintf("PAGERSCRIPT", WCS_STRING,
			"<SCRIPT LANGUAGE=\"JavaScript\">\n"
			"function launch_page_popup() {\n"
			"pwin = window.open('/page_popup', 'CitaPage%d', "
			"'toolbar=no,location=no,copyhistory=no,status=no,"
			"scrollbars=yes,resizable=no,height=250,width=400');\n"
			"}\n"
			"function force_frameset() { \n"
			" if (top.frames.length == 0) { \n"
			"  top.location.replace('/do_welcome'); \n"
			" } \n"
			"} \n"
			"%s\n"
			"</SCRIPT>\n",
			++pageseq,
			onload_fcn
		);
		/* end script */


		do_template("head");
		clear_local_substs();

		svprintf("extrabodyparms", WCS_STRING, "%s", 
			"onload='onload_fcn();' ");

		do_template("background");
		clear_local_substs();
	}

	if (print_standard_html_head == 1) {
		wprintf("<A NAME=\"TheTop\"></A>");
		embed_room_banner(NULL);
	}

	if (strlen(WC->ImportantMessage) > 0) {
		do_template("beginbox_nt");
		wprintf("<SPAN CLASS=\"errormsg\">"
			"%s</SPAN><BR>\n", WC->ImportantMessage);
		do_template("endbox");
		strcpy(WC->ImportantMessage, "");
	}	
}


/*
 *
 */
void http_redirect(char *whichpage) {
	wprintf("HTTP/1.0 302 Moved Temporarily\n");
	wprintf("Location: %s\n", whichpage);
	wprintf("URI: %s\n", whichpage);
	wprintf("Content-type: text/html\n\n");
	wprintf("<html><body>\n");
	wprintf("you really want to be <A HREF=\"%s\">here</A> now\n",
		whichpage);
	wprintf("</body></html>\n");
}



void check_for_express_messages()
{
	char buf[SIZ];

	serv_puts("NOOP");
	serv_gets(buf);
	if (buf[3] == '*') WC->HaveExpressMessages = 1;
}




/* 
 * Output a piece of content to the web browser
 */
void http_transmit_thing(char *thing, size_t length, char *content_type,
			 int is_static) {
	output_headers(is_static ? 0x10 : 0x00);
	wprintf("Content-type: %s\n"
		"Content-length: %ld\n"
		"Server: %s\n"
		"Connection: close\n"
		"\n",
		content_type,
		(long) length,
		SERVER
	);
	client_write(thing, (size_t)length);
}




void output_static(char *what)
{
	char buf[4096];
	FILE *fp;
	struct stat statbuf;
	off_t bytes;
	char *bigbuffer;
	char content_type[SIZ];

	sprintf(buf, "static/%s", what);
	fp = fopen(buf, "rb");
	if (fp == NULL) {
		wprintf("HTTP/1.0 404 %s\n", strerror(errno));
		wprintf("Content-Type: text/plain\n");
		wprintf("\n");
		wprintf("Cannot open %s: %s\n", what, strerror(errno));
	} else {
		if (!strncasecmp(&what[strlen(what) - 4], ".gif", 4))
			strcpy(content_type, "image/gif");
		else if (!strncasecmp(&what[strlen(what) - 4], ".txt", 4))
			strcpy(content_type, "text/plain");
		else if (!strncasecmp(&what[strlen(what) - 4], ".css", 4))
			strcpy(content_type, "text/css");
		else if (!strncasecmp(&what[strlen(what) - 4], ".jpg", 4))
			strcpy(content_type, "image/jpeg");
		else if (!strncasecmp(&what[strlen(what) - 4], ".png", 4))
			strcpy(content_type, "image/png");
		else if (!strncasecmp(&what[strlen(what) - 4], ".ico", 4))
			strcpy(content_type, "image/x-icon");
		else if (!strncasecmp(&what[strlen(what) - 5], ".html", 5))
			strcpy(content_type, "text/html");
		else if (!strncasecmp(&what[strlen(what) - 4], ".htm", 4))
			strcpy(content_type, "text/html");
		else if (!strncasecmp(&what[strlen(what) - 4], ".wml", 4))
			strcpy(content_type, "text/vnd.wap.wml");
		else if (!strncasecmp(&what[strlen(what) - 5], ".wmls", 5))
			strcpy(content_type, "text/vnd.wap.wmlscript");
		else if (!strncasecmp(&what[strlen(what) - 5], ".wmlc", 5))
			strcpy(content_type, "application/vnd.wap.wmlc");
		else if (!strncasecmp(&what[strlen(what) - 6], ".wmlsc", 6))
			strcpy(content_type, "application/vnd.wap.wmlscriptc");
		else if (!strncasecmp(&what[strlen(what) - 5], ".wbmp", 5))
			strcpy(content_type, "image/vnd.wap.wbmp");
		else if (!strncasecmp(&what[strlen(what) - 3], ".js", 3))
			strcpy(content_type, "text/javascript");
		else
			strcpy(content_type, "application/octet-stream");

		fstat(fileno(fp), &statbuf);
		bytes = statbuf.st_size;
		lprintf(3, "Static: %s, (%s; %ld bytes)\n", what, content_type, bytes);
		bigbuffer = malloc(bytes + 2);
		fread(bigbuffer, bytes, 1, fp);
		fclose(fp);

		http_transmit_thing(bigbuffer, (size_t)bytes, content_type, 1);
		free(bigbuffer);
	}
	if (!strcasecmp(bstr("force_close_session"), "yes")) {
		end_webcit_session();
	}
}


/*
 * When the browser requests an image file from the Citadel server,
 * this function is called to transmit it.
 */
void output_image()
{
	char buf[SIZ];
	char *xferbuf = NULL;
	off_t bytes;

	serv_printf("OIMG %s|%s", bstr("name"), bstr("parm"));
	serv_gets(buf);
	if (buf[0] == '2') {
		bytes = extract_long(&buf[4], 0);
		xferbuf = malloc(bytes + 2);

		/* Read it from the server */
		read_server_binary(xferbuf, bytes);
		serv_puts("CLOS");
		serv_gets(buf);

		/* Write it to the browser */
		http_transmit_thing(xferbuf, (size_t)bytes, "image/gif", 0);
		free(xferbuf);

	} else {

		/* Instead of an ugly 404, send a 1x1 transparent GIF
		 * when there's no such image on the server.
		 */
		output_static("blank.gif");

		/*
		wprintf("HTTP/1.0 404 %s\n", &buf[4]);
		output_headers(0);
		wprintf("Content-Type: text/plain\n"
			"\n"
			"Error retrieving image: %s\n",
			&buf[4]
		);
		*/

	}



}

/*
 */
void output_mimepart()
{
	char buf[SIZ];
	off_t bytes;
	char content_type[SIZ];
	char *content = NULL;
	
	serv_printf("OPNA %s|%s", bstr("msgnum"), bstr("partnum"));
	serv_gets(buf);
	if (buf[0] == '2') {
		bytes = extract_long(&buf[4], 0);
		content = malloc(bytes + 2);
		extract(content_type, &buf[4], 3);
		output_headers(0);
		read_server_binary(content, bytes);
		serv_puts("CLOS");
		serv_gets(buf);
		http_transmit_thing(content, bytes, content_type, 0);
		free(content);
	} else {
		wprintf("HTTP/1.0 404 %s\n", &buf[4]);
		output_headers(0);
		wprintf("Content-Type: text/plain\n");
		wprintf("\n");
		wprintf("Error retrieving part: %s\n", &buf[4]);
	}

}


/*
 */
char *load_mimepart(long msgnum, char *partnum)
{
	char buf[SIZ];
	off_t bytes;
	char content_type[SIZ];
	char *content;
	
	serv_printf("OPNA %ld|%s", msgnum, partnum);
	serv_gets(buf);
	if (buf[0] == '2') {
		bytes = extract_long(&buf[4], 0);
		extract(content_type, &buf[4], 3);

		content = malloc(bytes + 2);
		read_server_binary(content, bytes);

		serv_puts("CLOS");
		serv_gets(buf);
		content[bytes] = 0;	/* null terminate for good measure */
		return(content);
	}
	else {
		return(NULL);
	}

}


/*
 * Convenience functions to display a page containing only a string
 */
void convenience_page(char *titlebarcolor, char *titlebarmsg, char *messagetext)
{
	wprintf("HTTP/1.0 200 OK\n");
	output_headers(1);
	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#%s\"><TR><TD>", titlebarcolor);
	wprintf("<SPAN CLASS=\"titlebar\">%s</SPAN>\n", titlebarmsg);
	wprintf("</TD></TR></TABLE><BR>\n");
	escputs(messagetext);

	wprintf("<HR>\n");
	wDumpContent(1);
}


/*
 * Display a blank page.
 */
void blank_page(void) {
	output_headers(7);
	wDumpContent(2);
}


/*
 * A template has been requested
 */
void url_do_template(void) {
	do_template(bstr("template"));
}



/*
 * Offer to make any page the user's "start page."
 */
void offer_start_page(void) {
	wprintf("<A HREF=\"/change_start_page?startpage=");
	urlescputs(WC->this_page);
	wprintf("\">"
		"<FONT SIZE=-2 COLOR=\"#AAAAAA\">Make this my start page</FONT>"
		"</A>"
	);
}


/* 
 * Change the user's start page
 */
void change_start_page(void) {

	if (bstr("startpage") == NULL) {
		strcpy(WC->ImportantMessage,
			"startpage set to null");
		display_main_menu();
		return;
	}

	set_preference("startpage", bstr("startpage"));

	output_headers(3);
	do_template("newstartpage");
	wDumpContent(1);
}




void display_success(char *successmessage)
{
	convenience_page("007700", "OK", successmessage);
}



void extract_action(char *actbuf, char *cmdbuf)
{
	int i;

	strcpy(actbuf, cmdbuf);
	if (!strncasecmp(actbuf, "GET /", 5))
		strcpy(actbuf, &actbuf[5]);
	if (!strncasecmp(actbuf, "PUT /", 5))
		strcpy(actbuf, &actbuf[5]);
	if (!strncasecmp(actbuf, "POST /", 6))
		strcpy(actbuf, &actbuf[6]);

	for (i = 0; i < strlen(actbuf); ++i) {
		if (actbuf[i] == ' ') {
			actbuf[i] = 0;
			i = 0;
		}
		if (actbuf[i] == '/') {
			actbuf[i] = 0;
			i = 0;
		}
		if (actbuf[i] == '?') {
			actbuf[i] = 0;
			i = 0;
		}
		if (actbuf[i] == '&') {
			actbuf[i] = 0;
			i = 0;
		}
	}
}


void upload_handler(char *name, char *filename, char *partnum, char *disp,
			void *content, char *cbtype, size_t length,
			char *encoding, void *userdata)
{
	struct urlcontent *u;

	/* Form fields */
	if ( (length > 0) && (strlen(cbtype) == 0) ) {
		u = (struct urlcontent *) malloc(sizeof(struct urlcontent));
		u->next = WC->urlstrings;
		WC->urlstrings = u;
		safestrncpy(u->url_key, name, sizeof(u->url_key));
		u->url_data = malloc(length + 1);
		memcpy(u->url_data, content, length);
		u->url_data[length] = 0;
	}

	/* Uploaded files */
	if ( (length > 0) && (strlen(cbtype) > 0) ) {
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




/*
 * Entry point for WebCit transaction
 */
void session_loop(struct httprequest *req)
{
	char cmd[SIZ];
	char action[SIZ];
	char buf[SIZ];
	int a, b;
	int ContentLength = 0;
	int BytesRead = 0;
	char ContentType[512];
	char *content;
	char *content_end;
	struct httprequest *hptr;
	char browser_host[SIZ];
	char user_agent[SIZ];
	int body_start;

	/* We stuff these with the values coming from the client cookies,
	 * so we can use them to reconnect a timed out session if we have to.
	 */
	char c_username[SIZ];
	char c_password[SIZ];
	char c_roomname[SIZ];
	char cookie[SIZ];

	strcpy(c_username, "");
	strcpy(c_password, "");
	strcpy(c_roomname, "");

	WC->upload_length = 0;
	WC->upload = NULL;

	WC->is_wap = 0;

	hptr = req;
	if (hptr == NULL) return;

	strcpy(cmd, hptr->line);
	hptr = hptr->next;
	extract_action(action, cmd);

	while (hptr != NULL) {
		strcpy(buf, hptr->line);
		hptr = hptr->next;

		if (!strncasecmp(buf, "Cookie: webcit=", 15)) {
			strcpy(cookie, &buf[15]);
			cookie_to_stuff(cookie, NULL,
				      c_username, c_password, c_roomname);
		}
		else if (!strncasecmp(buf, "Content-length: ", 16)) {
			ContentLength = atoi(&buf[16]);
		}
		else if (!strncasecmp(buf, "Content-type: ", 14)) {
			safestrncpy(ContentType, &buf[14], sizeof ContentType);
		}
		else if (!strncasecmp(buf, "User-agent: ", 12)) {
			safestrncpy(user_agent, &buf[12], sizeof user_agent);
		}
		else if (!strncasecmp(buf, "Host: ", 6)) {
			safestrncpy(WC->http_host, &buf[6], sizeof WC->http_host);
		}
		/* Only WAP gateways explicitly name this content-type */
		else if (strstr(buf, "text/vnd.wap.wml")) {
			WC->is_wap = 1;
		}
	}

	if (ContentLength > 0) {
		content = malloc(ContentLength + SIZ);
		memset(content, 0, ContentLength + SIZ);
		sprintf(content, "Content-type: %s\n"
				"Content-length: %d\n\n",
				ContentType, ContentLength);
		body_start = strlen(content);

/***** old version
		BytesRead = 0;
		while (BytesRead < ContentLength) {
			a=read(WC->http_sock, &content[BytesRead+body_start],
				ContentLength - BytesRead);
			if (a <= 0) BytesRead = ContentLength;
			else BytesRead += a;
		}
*******/

		/* Now we're daring and read it all at once. */
		client_read(WC->http_sock, &content[BytesRead+body_start], ContentLength);

		if (!strncasecmp(ContentType,
			      "application/x-www-form-urlencoded", 33)) {
			addurls(&content[body_start]);
		} else if (!strncasecmp(ContentType, "multipart", 9)) {
			content_end = content + ContentLength + body_start;
			mime_parser(content, content_end, *upload_handler,
					NULL, NULL, NULL, 0);
		}
	} else {
		content = NULL;
	}

	/* make a note of where we are in case the user wants to save it */
	safestrncpy(WC->this_page, cmd, sizeof(WC->this_page));
	remove_token(WC->this_page, 2, ' ');
	remove_token(WC->this_page, 0, ' ');

	/* If there are variables in the URL, we must grab them now */
	for (a = 0; a < strlen(cmd); ++a) {
		if ((cmd[a] == '?') || (cmd[a] == '&')) {
			for (b = a; b < strlen(cmd); ++b)
				if (isspace(cmd[b]))
					cmd[b] = 0;
			addurls(&cmd[a + 1]);
			cmd[a] = 0;
		}
	}

	/* Static content can be sent without connecting to Citadel. */
	if (!strcasecmp(action, "static")) {
		strcpy(buf, &cmd[12]);
		for (a = 0; a < strlen(buf); ++a)
			if (isspace(buf[a]))
				buf[a] = 0;
		output_static(buf);
		goto SKIP_ALL_THIS_CRAP;	/* Don't try to connect */
	}

	/*
	 * If we're not connected to a Citadel server, try to hook up the
	 * connection now.
	 */
	if (!WC->connected) {
		if (!strcasecmp(ctdlhost, "uds")) {
			/* unix domain socket */
			sprintf(buf, "%s/citadel.socket", ctdlport);
			WC->serv_sock = uds_connectsock(buf);
		}
		else {
			/* tcp socket */
			WC->serv_sock = tcp_connectsock(ctdlhost, ctdlport);
		}

		if (WC->serv_sock < 0) {
			do_logout();
			goto SKIP_ALL_THIS_CRAP;
		}
		else {
			WC->connected = 1;
			serv_gets(buf);	/* get the server welcome message */
			locate_host(browser_host, WC->http_sock);
			get_serv_info(browser_host, user_agent);
			if (serv_info.serv_rev_level < MINIMUM_CIT_VERSION) {
				wprintf("You are connected to a Citadel "
					"server running Citadel %d.%02d;\nin "
					"order to run this version of WebCit "
					"you must also have Citadel %d.%02d or"
					" newer.\n\n\n",
						serv_info.serv_rev_level / 100,
						serv_info.serv_rev_level % 100,
						MINIMUM_CIT_VERSION / 100,
						MINIMUM_CIT_VERSION % 100
					);
				end_webcit_session();
				goto SKIP_ALL_THIS_CRAP;
			}
		}
	}

	/*
	 * Functions which can be performed without logging in
	 */
	if (!strcasecmp(action, "listsub")) {
		do_listsub();
		goto SKIP_ALL_THIS_CRAP;
	}
#ifdef WEBCIT_WITH_CALENDAR_SERVICE
	if (!strcasecmp(action, "freebusy")) {
		do_freebusy(cmd);
		goto SKIP_ALL_THIS_CRAP;
	}
#endif

	check_for_express_messages();

	/*
	 * If we're not logged in, but we have username and password cookies
	 * supplied by the browser, try using them to log in.
	 */
	if ((!WC->logged_in) && (strlen(c_username) > 0) && (strlen(c_password) > 0)) {
		serv_printf("USER %s", c_username);
		serv_gets(buf);
		if (buf[0] == '3') {
			serv_printf("PASS %s", c_password);
			serv_gets(buf);
			if (buf[0] == '2') {
				become_logged_in(c_username, c_password, buf);
			}
		}
	}
	/*
	 * If we don't have a current room, but a cookie specifying the
	 * current room is supplied, make an effort to go there.
	 */
	if ((strlen(WC->wc_roomname) == 0) && (strlen(c_roomname) > 0)) {
		serv_printf("GOTO %s", c_roomname);
		serv_gets(buf);
		if (buf[0] == '2') {
			strcpy(WC->wc_roomname, c_roomname);
		}
	}

	if (!strcasecmp(action, "image")) {
		output_image();

	/*
	 * All functions handled below this point ... make sure we log in
	 * before doing anything else!
	 */
	} else if ((!WC->logged_in) && (!strcasecmp(action, "login"))) {
		do_login();
	} else if (!WC->logged_in) {
		display_login(NULL);
	}

	/*
	 * Various commands...
	 */

	else if (!strcasecmp(action, "do_welcome")) {
		do_welcome();
	} else if (!strcasecmp(action, "blank")) {
		blank_page();
	} else if (!strcasecmp(action, "do_template")) {
		url_do_template();
	} else if (!strcasecmp(action, "display_main_menu")) {
		display_main_menu();
	} else if (!strcasecmp(action, "whobbs")) {
		whobbs();
	} else if (!strcasecmp(action, "knrooms")) {
		knrooms();
	} else if (!strcasecmp(action, "gotonext")) {
		slrp_highest();
		gotonext();
	} else if (!strcasecmp(action, "skip")) {
		gotonext();
	} else if (!strcasecmp(action, "ungoto")) {
		ungoto();
	} else if (!strcasecmp(action, "dotgoto")) {
		slrp_highest();
		smart_goto(bstr("room"));
	} else if (!strcasecmp(action, "dotskip")) {
		smart_goto(bstr("room"));
	} else if (!strcasecmp(action, "termquit")) {
		do_logout();
	} else if (!strcasecmp(action, "readnew")) {
		readloop("readnew");
	} else if (!strcasecmp(action, "readold")) {
		readloop("readold");
	} else if (!strcasecmp(action, "readfwd")) {
		readloop("readfwd");
	} else if (!strcasecmp(action, "headers")) {
		readloop("headers");
	} else if (!strcasecmp(action, "display_enter")) {
		display_enter();
	} else if (!strcasecmp(action, "post")) {
		post_message();
	} else if (!strcasecmp(action, "do_stuff_to_one_msg")) {
		do_stuff_to_one_msg();
	} else if (!strcasecmp(action, "move_msg")) {
		move_msg();
	} else if (!strcasecmp(action, "userlist")) {
		userlist();
	} else if (!strcasecmp(action, "showuser")) {
		showuser();
	} else if (!strcasecmp(action, "display_page")) {
		display_page();
	} else if (!strcasecmp(action, "page_user")) {
		page_user();
	} else if (!strcasecmp(action, "chat")) {
		do_chat();
	} else if (!strcasecmp(action, "display_private")) {
		display_private("", 0);
	} else if (!strcasecmp(action, "goto_private")) {
		goto_private();
	} else if (!strcasecmp(action, "zapped_list")) {
		zapped_list();
	} else if (!strcasecmp(action, "display_zap")) {
		display_zap();
	} else if (!strcasecmp(action, "zap")) {
		zap();
	} else if (!strcasecmp(action, "display_entroom")) {
		display_entroom();
	} else if (!strcasecmp(action, "entroom")) {
		entroom();
	} else if (!strcasecmp(action, "display_editroom")) {
		display_editroom();
	} else if (!strcasecmp(action, "netedit")) {
		netedit();
	} else if (!strcasecmp(action, "editroom")) {
		editroom();
        } else if (!strcasecmp(action, "display_whok")) {
                display_whok();
	} else if (!strcasecmp(action, "display_editinfo")) {
		display_edit("Room info", "EINF 0", "RINF", "/editinfo", 1);
	} else if (!strcasecmp(action, "editinfo")) {
		save_edit("Room info", "EINF 1", 1);
	} else if (!strcasecmp(action, "display_editbio")) {
		sprintf(buf, "RBIO %s", WC->wc_username);
		display_edit("Your bio", "NOOP", buf, "editbio", 3);
	} else if (!strcasecmp(action, "editbio")) {
		save_edit("Your bio", "EBIO", 0);
	} else if (!strcasecmp(action, "confirm_delete_room")) {
		confirm_delete_room();
	} else if (!strcasecmp(action, "delete_room")) {
		delete_room();
	} else if (!strcasecmp(action, "validate")) {
		validate();
	} else if (!strcasecmp(action, "display_editpic")) {
		display_graphics_upload("your photo",
					"UIMG 0|_userpic_",
					"/editpic");
	} else if (!strcasecmp(action, "editpic")) {
		do_graphics_upload("UIMG 1|_userpic_");
	} else if (!strcasecmp(action, "display_editroompic")) {
		display_graphics_upload("the graphic for this room",
					"UIMG 0|_roompic_",
					"/editroompic");
	} else if (!strcasecmp(action, "editroompic")) {
		do_graphics_upload("UIMG 1|_roompic_");
	} else if (!strcasecmp(action, "delete_floor")) {
		delete_floor();
	} else if (!strcasecmp(action, "rename_floor")) {
		rename_floor();
	} else if (!strcasecmp(action, "create_floor")) {
		create_floor();
	} else if (!strcasecmp(action, "display_editfloorpic")) {
		sprintf(buf, "UIMG 0|_floorpic_|%s",
			bstr("which_floor"));
		display_graphics_upload("the graphic for this floor",
					buf,
					"/editfloorpic");
	} else if (!strcasecmp(action, "editfloorpic")) {
		sprintf(buf, "UIMG 1|_floorpic_|%s",
			bstr("which_floor"));
		do_graphics_upload(buf);
	} else if (!strcasecmp(action, "display_reg")) {
		display_reg(0);
	} else if (!strcasecmp(action, "display_changepw")) {
		display_changepw();
	} else if (!strcasecmp(action, "changepw")) {
		changepw();
	} else if (!strcasecmp(action, "display_edit_node")) {
		display_edit_node();
	} else if (!strcasecmp(action, "edit_node")) {
		edit_node();
	} else if (!strcasecmp(action, "display_netconf")) {
		display_netconf();
	} else if (!strcasecmp(action, "display_confirm_delete_node")) {
		display_confirm_delete_node();
	} else if (!strcasecmp(action, "delete_node")) {
		delete_node();
	} else if (!strcasecmp(action, "display_add_node")) {
		display_add_node();
	} else if (!strcasecmp(action, "add_node")) {
		add_node();
	} else if (!strcasecmp(action, "terminate_session")) {
		slrp_highest();
		terminate_session();
	} else if (!strcasecmp(action, "edit_me")) {
		edit_me();
	} else if (!strcasecmp(action, "display_siteconfig")) {
		display_siteconfig();
	} else if (!strcasecmp(action, "chat_recv")) {
		chat_recv();
	} else if (!strcasecmp(action, "chat_send")) {
		chat_send();
	} else if (!strcasecmp(action, "page_popup")) {
		page_popup();
	} else if (!strcasecmp(action, "siteconfig")) {
		siteconfig();
	} else if (!strcasecmp(action, "display_generic")) {
		display_generic();
	} else if (!strcasecmp(action, "do_generic")) {
		do_generic();
	} else if (!strcasecmp(action, "display_menubar")) {
		display_menubar(1);
	} else if (!strcasecmp(action, "output_mimepart")) {
		output_mimepart();
	} else if (!strcasecmp(action, "edit_vcard")) {
		edit_vcard();
	} else if (!strcasecmp(action, "submit_vcard")) {
		submit_vcard();
	} else if (!strcasecmp(action, "select_user_to_edit")) {
		select_user_to_edit(NULL, NULL);
	} else if (!strcasecmp(action, "display_edituser")) {
		display_edituser(NULL, 0);
	} else if (!strcasecmp(action, "edituser")) {
		edituser();
	} else if (!strcasecmp(action, "create_user")) {
		create_user();
	} else if (!strcasecmp(action, "changeview")) {
		change_view();
	} else if (!strcasecmp(action, "do_stuff_to_msgs")) {
		do_stuff_to_msgs();
	} else if (!strcasecmp(action, "change_start_page")) {
		change_start_page();
	} else if (!strcasecmp(action, "display_floorconfig")) {
		display_floorconfig(NULL);
	} else if (!strcasecmp(action, "toggle_self_service")) {
		toggle_self_service();
#ifdef WEBCIT_WITH_CALENDAR_SERVICE
	} else if (!strcasecmp(action, "display_edit_task")) {
		display_edit_task();
	} else if (!strcasecmp(action, "save_task")) {
		save_task();
	} else if (!strcasecmp(action, "display_edit_event")) {
		display_edit_event();
	} else if (!strcasecmp(action, "save_event")) {
		save_event();
	} else if (!strcasecmp(action, "respond_to_request")) {
		respond_to_request();
	} else if (!strcasecmp(action, "handle_rsvp")) {
		handle_rsvp();
#endif
	} else if (!strcasecmp(action, "summary")) {
		summary();
	} else if (!strcasecmp(action, "iconbar")) {
		do_iconbar();
	} else if (!strcasecmp(action, "display_customize_iconbar")) {
		display_customize_iconbar();
	} else if (!strcasecmp(action, "commit_iconbar")) {
		commit_iconbar();
	} else if (!strcasecmp(action, "set_room_policy")) {
		set_room_policy();
	} else if (!strcasecmp(action, "display_inetconf")) {
		display_inetconf();
	} else if (!strcasecmp(action, "save_inetconf")) {
		save_inetconf();
	} else if (!strcasecmp(action, "diagnostics")) {
		output_headers(1);

		wprintf("You're in session %d<HR>\n", WC->wc_session);
		wprintf("Command: <BR><PRE>\n");
		escputs(cmd);
		wprintf("</PRE><HR>\n");
		wprintf("Variables: <BR><PRE>\n");
		dump_vars();
		wprintf("</PRE><HR>\n");
		wDumpContent(1);
	}
	/* When all else fais, display the main menu. */
	else {
		display_main_menu();
	}

SKIP_ALL_THIS_CRAP:
	fflush(stdout);
	if (content != NULL) {
		free(content);
		content = NULL;
	}
	free_urls();
	if (WC->upload_length > 0) {
		free(WC->upload);
		WC->upload_length = 0;
	}

}
