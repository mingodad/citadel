/*
 * $Id$
 *
 * This is the main transaction loop of the web service.  It maintains a
 * persistent session to the Citadel server, handling HTTP WebCit requests as
 * they arrive and presenting a user interface.
 *
 */

#include "webcit.h"
#include "groupdav.h"
#include "webserver.h"
#include "mime_parser.h"

/*
 * Subdirectories from which the client may request static content
 */
char *static_content_dirs[] = {
	"static",
	"tiny_mce"
};

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
			b = 0;
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
		safestrncpy(u->url_key, buf, sizeof u->url_key);

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
		safestrncpy(u->url_data, up, strlen(up) + 1);
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
		wprintf("</div>\n");	/* end of "text" div */
		do_template("trailing");
	}

	/* If we've been saving it all up for one big output burst,
	 * go ahead and do that now.
	 */
	end_burst();
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
	char *ec = " #&;`'|*?-~<>^()[]{}$\"\\";

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
		if (strbuf[a] == '\n')
			strcat(target, " ");
		else if (strbuf[a] == '\r')
			strcat(target, " ");
		else if (strbuf[a] == '\'')
			strcat(target, "&#39;");
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
 */
void output_headers(	int do_httpheaders,	/* 1 = output HTTP headers                          */
			int do_htmlhead,	/* 1 = output HTML <head> section and <body> opener */

			int do_room_banner,	/* 0=no, 1=yes,                                     */
						/* 2 = I'm going to embed my own, so don't open the */
						/*     <div id="content"> either.                   */

			int unset_cookies,	/* 1 = session is terminating, so unset the cookies */
			int suppress_check,	/* 1 = suppress check for instant messages          */
			int cache		/* 1 = allow browser to cache this page             */
) {
	char cookie[SIZ];
	char httpnow[SIZ];

	wprintf("HTTP/1.1 200 OK\n");
	httpdate(httpnow, time(NULL));

	if (do_httpheaders) {
		wprintf("Content-type: text/html; charset=utf-8\r\n"
			"Server: %s / %s\n"
			"Connection: close\r\n",
			SERVER, serv_info.serv_software
		);
	}

	if (cache) {
		wprintf("Pragma: public\r\n"
			"Cache-Control: max-age=3600, must-revalidate\r\n"
			"Last-modified: %s\r\n",
			httpnow
		);
	}
	else {
		wprintf("Pragma: no-cache\r\n"
			"Cache-Control: no-store\r\n"
		);
	}

	stuff_to_cookie(cookie, WC->wc_session, WC->wc_username,
			WC->wc_password, WC->wc_roomname);

	if (unset_cookies) {
		wprintf("Set-cookie: webcit=%s; path=/\r\n", unset);
	} else {
		wprintf("Set-cookie: webcit=%s; path=/\r\n", cookie);
		if (server_cookie != NULL) {
			wprintf("%s\n", server_cookie);
		}
	}

	if (do_htmlhead) {
		/* wprintf("\n"); */
		begin_burst();
		do_template("head");
	}

	/* ICONBAR */
	if (do_htmlhead) {

		if (WC->HaveInstantMessages) {
			wprintf("<div id=\"page_popup\">\n");
			page_popup();
			wprintf("</div>\n");
		}
		if (strlen(WC->ImportantMessage) > 0) {
			wprintf("<div id=\"important_message\">\n");
			wprintf("<SPAN CLASS=\"imsg\">"
				"%s</SPAN><br />\n", WC->ImportantMessage);
			wprintf("</div>\n");
			wprintf("<script type=\"text/javascript\">\n"
				"	 setTimeout('hide_imsg_popup()', 3000);	\n"
				"</script>\n");
			safestrncpy(WC->ImportantMessage, "", sizeof WC->ImportantMessage);
		}
		if ( (WC->logged_in) && (!unset_cookies) ) {
			wprintf("<div id=\"iconbar\">");
			do_iconbar();
			wprintf("</div>\n");
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


/*
 * Generic function to do an HTTP redirect.  Easy and fun.
 */
void http_redirect(char *whichpage) {
	wprintf("HTTP/1.1 302 Moved Temporarily\n");
	wprintf("Location: %s\r\n", whichpage);
	wprintf("URI: %s\r\n", whichpage);
	wprintf("Content-type: text/html; charset=utf-8\r\n\r\n");
	wprintf("<html><body>");
	wprintf("Go <a href=\"%s\">here</A>.", whichpage);
	wprintf("</body></html>\n");
}



void check_for_instant_messages()
{
	char buf[SIZ];

	serv_puts("NOOP");
	serv_getln(buf, sizeof buf);
	if (buf[3] == '*') WC->HaveInstantMessages = 1;
}




/* 
 * Output a piece of content to the web browser
 */
void http_transmit_thing(char *thing, size_t length, char *content_type,
			 int is_static) {

	output_headers(0, 0, 0, 0, 0, is_static);

	wprintf("Content-type: %s\r\n"
		"Server: %s\r\n"
		"Connection: close\r\n",
		content_type,
		SERVER);

#ifdef HAVE_ZLIB
	/* If we can send the data out compressed, please do so. */
	if (WC->gzip_ok) {
		char *compressed_data = NULL;
		uLongf compressed_len;

		compressed_len = (uLongf) ((length * 101) / 100) + 100;
		compressed_data = malloc(compressed_len);

		if (compress_gzip((Bytef *) compressed_data,
				  &compressed_len,
				  (Bytef *) thing,
				  (uLongf) length, Z_BEST_SPEED) == Z_OK) {
			wprintf("Content-encoding: gzip\r\n"
				"Content-length: %ld\r\n"
				"\r\n",
				(long) compressed_len
			);
			client_write(compressed_data, (size_t)compressed_len);
			free(compressed_data);
			return;
		}
	}
#endif

	/* No compression ... just send it out as-is */
	wprintf("Content-length: %ld\r\n"
		"\r\n",
		(long) length
	);
	client_write(thing, (size_t)length);
}




void output_static(char *what)
{
	FILE *fp;
	struct stat statbuf;
	off_t bytes;
	char *bigbuffer;
	char content_type[128];

	fp = fopen(what, "rb");
	if (fp == NULL) {
		lprintf(9, "output_static('%s')  -- NOT FOUND --\n", what);
		wprintf("HTTP/1.1 404 %s\n", strerror(errno));
		wprintf("Content-Type: text/plain\r\n");
		wprintf("\r\n");
		wprintf("Cannot open %s: %s\n", what, strerror(errno));
	} else {
		if (!strncasecmp(&what[strlen(what) - 4], ".gif", 4))
			safestrncpy(content_type, "image/gif", sizeof content_type);
		else if (!strncasecmp(&what[strlen(what) - 4], ".txt", 4))
			safestrncpy(content_type, "text/plain", sizeof content_type);
		else if (!strncasecmp(&what[strlen(what) - 4], ".css", 4))
			safestrncpy(content_type, "text/css", sizeof content_type);
		else if (!strncasecmp(&what[strlen(what) - 4], ".jpg", 4))
			safestrncpy(content_type, "image/jpeg", sizeof content_type);
		else if (!strncasecmp(&what[strlen(what) - 4], ".png", 4))
			safestrncpy(content_type, "image/png", sizeof content_type);
		else if (!strncasecmp(&what[strlen(what) - 4], ".ico", 4))
			safestrncpy(content_type, "image/x-icon", sizeof content_type);
		else if (!strncasecmp(&what[strlen(what) - 5], ".html", 5))
			safestrncpy(content_type, "text/html", sizeof content_type);
		else if (!strncasecmp(&what[strlen(what) - 4], ".htm", 4))
			safestrncpy(content_type, "text/html", sizeof content_type);
		else if (!strncasecmp(&what[strlen(what) - 4], ".wml", 4))
			safestrncpy(content_type, "text/vnd.wap.wml", sizeof content_type);
		else if (!strncasecmp(&what[strlen(what) - 5], ".wmls", 5))
			safestrncpy(content_type, "text/vnd.wap.wmlscript", sizeof content_type);
		else if (!strncasecmp(&what[strlen(what) - 5], ".wmlc", 5))
			safestrncpy(content_type, "application/vnd.wap.wmlc", sizeof content_type);
		else if (!strncasecmp(&what[strlen(what) - 6], ".wmlsc", 6))
			safestrncpy(content_type, "application/vnd.wap.wmlscriptc", sizeof content_type);
		else if (!strncasecmp(&what[strlen(what) - 5], ".wbmp", 5))
			safestrncpy(content_type, "image/vnd.wap.wbmp", sizeof content_type);
		else if (!strncasecmp(&what[strlen(what) - 3], ".js", 3))
			safestrncpy(content_type, "text/javascript", sizeof content_type);
		else
			safestrncpy(content_type, "application/octet-stream", sizeof content_type);

		fstat(fileno(fp), &statbuf);
		bytes = statbuf.st_size;
		bigbuffer = malloc(bytes + 2);
		fread(bigbuffer, bytes, 1, fp);
		fclose(fp);

		lprintf(9, "output_static('%s')  %s\n", what, content_type);
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
	serv_getln(buf, sizeof buf);
	if (buf[0] == '2') {
		bytes = extract_long(&buf[4], 0);
		xferbuf = malloc(bytes + 2);

		/* Read it from the server */
		read_server_binary(xferbuf, bytes);
		serv_puts("CLOS");
		serv_getln(buf, sizeof buf);

		/* Write it to the browser */
		http_transmit_thing(xferbuf, (size_t)bytes, "image/gif", 0);
		free(xferbuf);

	} else {
		/* Instead of an ugly 404, send a 1x1 transparent GIF
		 * when there's no such image on the server.
		 */
		output_static("static/blank.gif");
	}



}

/*
 * Generic function to output an arbitrary MIME part from an arbitrary
 * message number on the server.
 */
void mimepart(char *msgnum, char *partnum)
{
	char buf[SIZ];
	off_t bytes;
	char content_type[SIZ];
	char *content = NULL;
	
	serv_printf("OPNA %s|%s", msgnum, partnum);
	serv_getln(buf, sizeof buf);
	if (buf[0] == '2') {
		bytes = extract_long(&buf[4], 0);
		content = malloc(bytes + 2);
		extract_token(content_type, &buf[4], 3, '|', sizeof content_type);
		output_headers(0, 0, 0, 0, 0, 0);
		read_server_binary(content, bytes);
		serv_puts("CLOS");
		serv_getln(buf, sizeof buf);
		http_transmit_thing(content, bytes, content_type, 0);
		free(content);
	} else {
		wprintf("HTTP/1.1 404 %s\n", &buf[4]);
		output_headers(0, 0, 0, 0, 0, 0);
		wprintf("Content-Type: text/plain\r\n");
		wprintf("\r\n");
		wprintf(_("An error occurred while retrieving this part: %s\n"), &buf[4]);
	}

}


/*
 * Read any MIME part of a message, from the server, into memory.
 */
char *load_mimepart(long msgnum, char *partnum)
{
	char buf[SIZ];
	off_t bytes;
	char content_type[SIZ];
	char *content;
	
	serv_printf("OPNA %ld|%s", msgnum, partnum);
	serv_getln(buf, sizeof buf);
	if (buf[0] == '2') {
		bytes = extract_long(&buf[4], 0);
		extract_token(content_type, &buf[4], 3, '|', sizeof content_type);

		content = malloc(bytes + 2);
		read_server_binary(content, bytes);

		serv_puts("CLOS");
		serv_getln(buf, sizeof buf);
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
	wprintf("HTTP/1.1 200 OK\n");
	output_headers(1, 1, 2, 0, 0, 0);
	wprintf("<div id=\"banner\">\n");
	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#%s\"><TR><TD>", titlebarcolor);
	wprintf("<SPAN CLASS=\"titlebar\">%s</SPAN>\n", titlebarmsg);
	wprintf("</TD></TR></TABLE>\n");
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
	do_template(bstr("template"));
}



/*
 * Offer to make any page the user's "start page."
 */
void offer_start_page(void) {
	wprintf("<a href=\"change_start_page?startpage=");
	urlescputs(WC->this_page);
	wprintf("\"><FONT SIZE=-2 COLOR=\"#AAAAAA\">");
	wprintf(_("Make this my start page"));
	wprintf("</FONT></A>");
/*
	wprintf("<br/><a href=\"rss?room=");
	urlescputs(WC->wc_roomname);
	wprintf("\" title=\"RSS 2.0 feed for ");
	escputs(WC->wc_roomname);
	wprintf("\"><img alt=\"RSS\" border=\"0\" src=\"static/xml_button.gif\"/></a>\n");
*/
}


/* 
 * Change the user's start page
 */
void change_start_page(void) {

	if (bstr("startpage") == NULL) {
		safestrncpy(WC->ImportantMessage,
			_("You no longer have a start page selected."),
			sizeof WC->ImportantMessage);
		display_main_menu();
		return;
	}

	set_preference("startpage", bstr("startpage"), 1);

	output_headers(1, 1, 0, 0, 0, 0);
	do_template("newstartpage");
	wDumpContent(1);
}




void display_success(char *successmessage)
{
	convenience_page("007700", "OK", successmessage);
}


/* Authorization required page */
/* This is probably temporary and should be revisited */
void authorization_required(const char *message)
{
	wprintf("HTTP/1.1 401 Authorization Required\r\n");
	wprintf("WWW-Authenticate: Basic realm=\"\"\r\n", serv_info.serv_humannode);
	wprintf("Content-Type: text/html\r\n\r\n");
	wprintf("<h1>");
	wprintf(_("Authorization Required"));
	wprintf("</h1>\r\n");
	wprintf(_("The resource you requested requires a valid username and password. "
		"You could not be logged in: %s\n"), message);
	wDumpContent(0);
}


void upload_handler(char *name, char *filename, char *partnum, char *disp,
			void *content, char *cbtype, char *cbcharset,
			size_t length, char *encoding, void *userdata)
{
	struct urlcontent *u;

	/* lprintf(9, "upload_handler() name=%s, type=%s, len=%d\n",
		name, cbtype, length); */

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
 * Convenience functions to wrap around asynchronous ajax responses
 */
void begin_ajax_response(void) {
        output_headers(0, 0, 0, 0, 0, 0);

        wprintf("Content-type: text/html; charset=UTF-8\r\n"
                "Server: %s\r\n"
                "Connection: close\r\n"
                "Pragma: no-cache\r\n"
                "Cache-Control: no-cache\r\n",
                SERVER);
        begin_burst();
}

void end_ajax_response(void) {
        wprintf("\r\n");
        wDumpContent(0);
}

void ajax_servcmd(void)
{
	char buf[1024];
	char gcontent[1024];
	char *junk;
	size_t len;

	begin_ajax_response();

	serv_printf("%s", bstr("g_cmd"));
	serv_getln(buf, sizeof buf);

	if (buf[0] == '8') {
		serv_printf("\n\n000");
	}
	if ((buf[0] == '1') || (buf[0] == '8')) {
		while (serv_getln(gcontent, sizeof gcontent), strcmp(gcontent, "000")) {
			/* maybe do something with it? */
		}
		wprintf("000");
	}
	if (buf[0] == '4') {
		text_to_server(bstr("g_input"), 0);
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
}






/*
 * Entry point for WebCit transaction
 */
void session_loop(struct httprequest *req)
{
	char cmd[1024];
	char action[1024];
	char arg1[128];
	char arg2[128];
	char arg3[128];
	char arg4[128];
	char arg5[128];
	char arg6[128];
	char arg7[128];
	char buf[SIZ];
	char request_method[128];
	char pathname[1024];
	int a, b;
	int ContentLength = 0;
	int BytesRead = 0;
	char ContentType[512];
	char *content = NULL;
	char *content_end = NULL;
	struct httprequest *hptr;
	char browser_host[SIZ];
	char user_agent[SIZ];
	int body_start = 0;
	int is_static = 0;

	/* We stuff these with the values coming from the client cookies,
	 * so we can use them to reconnect a timed out session if we have to.
	 */
	char c_username[SIZ];
	char c_password[SIZ];
	char c_roomname[SIZ];
	char c_httpauth_string[SIZ];
	char c_httpauth_user[SIZ];
	char c_httpauth_pass[SIZ];
	char cookie[SIZ];

	safestrncpy(c_username, "", sizeof c_username);
	safestrncpy(c_password, "", sizeof c_password);
	safestrncpy(c_roomname, "", sizeof c_roomname);
	safestrncpy(c_httpauth_string, "", sizeof c_httpauth_string);
	safestrncpy(c_httpauth_user, DEFAULT_HTTPAUTH_USER, sizeof c_httpauth_user);
	safestrncpy(c_httpauth_pass, DEFAULT_HTTPAUTH_PASS, sizeof c_httpauth_pass);

	WC->upload_length = 0;
	WC->upload = NULL;
	WC->vars = NULL;

	WC->is_wap = 0;

	hptr = req;
	if (hptr == NULL) return;

	safestrncpy(cmd, hptr->line, sizeof cmd);
	hptr = hptr->next;
	extract_token(request_method, cmd, 0, ' ', sizeof request_method);
	extract_token(pathname, cmd, 1, ' ', sizeof pathname);

	/* Figure out the action */
	extract_token(action, pathname, 1, '/', sizeof action);
	if (strstr(action, "?")) *strstr(action, "?") = 0;
	if (strstr(action, "&")) *strstr(action, "&") = 0;
	if (strstr(action, " ")) *strstr(action, " ") = 0;

	extract_token(arg1, pathname, 2, '/', sizeof arg1);
	if (strstr(arg1, "?")) *strstr(arg1, "?") = 0;
	if (strstr(arg1, "&")) *strstr(arg1, "&") = 0;
	if (strstr(arg1, " ")) *strstr(arg1, " ") = 0;

	extract_token(arg2, pathname, 3, '/', sizeof arg2);
	if (strstr(arg2, "?")) *strstr(arg2, "?") = 0;
	if (strstr(arg2, "&")) *strstr(arg2, "&") = 0;
	if (strstr(arg2, " ")) *strstr(arg2, " ") = 0;

	extract_token(arg3, pathname, 4, '/', sizeof arg3);
	if (strstr(arg3, "?")) *strstr(arg3, "?") = 0;
	if (strstr(arg3, "&")) *strstr(arg3, "&") = 0;
	if (strstr(arg3, " ")) *strstr(arg3, " ") = 0;

	extract_token(arg4, pathname, 5, '/', sizeof arg4);
	if (strstr(arg4, "?")) *strstr(arg4, "?") = 0;
	if (strstr(arg4, "&")) *strstr(arg4, "&") = 0;
	if (strstr(arg4, " ")) *strstr(arg4, " ") = 0;

	extract_token(arg5, pathname, 6, '/', sizeof arg5);
	if (strstr(arg5, "?")) *strstr(arg5, "?") = 0;
	if (strstr(arg5, "&")) *strstr(arg5, "&") = 0;
	if (strstr(arg5, " ")) *strstr(arg5, " ") = 0;

	extract_token(arg6, pathname, 7, '/', sizeof arg6);
	if (strstr(arg6, "?")) *strstr(arg6, "?") = 0;
	if (strstr(arg6, "&")) *strstr(arg6, "&") = 0;
	if (strstr(arg6, " ")) *strstr(arg6, " ") = 0;

	extract_token(arg7, pathname, 8, '/', sizeof arg7);
	if (strstr(arg7, "?")) *strstr(arg7, "?") = 0;
	if (strstr(arg7, "&")) *strstr(arg7, "&") = 0;
	if (strstr(arg7, " ")) *strstr(arg7, " ") = 0;

	while (hptr != NULL) {
		safestrncpy(buf, hptr->line, sizeof buf);
		hptr = hptr->next;

		if (!strncasecmp(buf, "Cookie: webcit=", 15)) {
			safestrncpy(cookie, &buf[15], sizeof cookie);
			cookie_to_stuff(cookie, NULL,
					c_username, sizeof c_username,
					c_password, sizeof c_password,
					c_roomname, sizeof c_roomname);
		}
		else if (!strncasecmp(buf, "Authorization: Basic ", 21)) {
			CtdlDecodeBase64(c_httpauth_string, &buf[21], strlen(&buf[21]));
			extract_token(c_httpauth_user, c_httpauth_string, 0, ':', sizeof c_httpauth_user);
			extract_token(c_httpauth_pass, c_httpauth_string, 1, ':', sizeof c_httpauth_pass);
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

		/* Read the entire input data at once. */
		client_read(WC->http_sock, &content[BytesRead+body_start],
			ContentLength);

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
	is_static = 0;
	for (a=0; a<(sizeof(static_content_dirs) / sizeof(char *)); ++a) {
		if (!strcasecmp(action, static_content_dirs[a])) {
			is_static = 1;
		}
	}
	if (is_static) {
		snprintf(buf, sizeof buf, "%s/%s/%s/%s/%s/%s/%s/%s",
			action, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
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
			serv_getln(buf, sizeof buf);	/* get the server welcome message */
			locate_host(browser_host, WC->http_sock);
			get_serv_info(browser_host, user_agent);
			if (serv_info.serv_rev_level < MINIMUM_CIT_VERSION) {
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

	/*
	 * If we're not logged in, but we have HTTP Authentication data,
	 * try logging in to Citadel using that.
	 */
	if ((!WC->logged_in)
	   && (strlen(c_httpauth_user) > 0)
	   && (strlen(c_httpauth_pass) > 0)) {
		serv_printf("USER %s", c_httpauth_user);
		serv_getln(buf, sizeof buf);
		if (buf[0] == '3') {
			serv_printf("PASS %s", c_httpauth_pass);
			serv_getln(buf, sizeof buf);
			if (buf[0] == '2') {
				become_logged_in(c_httpauth_user,
						c_httpauth_pass, buf);
				safestrncpy(WC->httpauth_user, c_httpauth_user, sizeof WC->httpauth_user);
				safestrncpy(WC->httpauth_pass, c_httpauth_pass, sizeof WC->httpauth_pass);
			} else {
				/* Should only display when password is wrong */
				authorization_required(&buf[4]);
				goto SKIP_ALL_THIS_CRAP;
			}
		}
	}

	/* This needs to run early */
	if (!strcasecmp(action, "rss")) {
		display_rss(bstr("room"), request_method);
		goto SKIP_ALL_THIS_CRAP;
	}

	/* 
	 * The GroupDAV stuff relies on HTTP authentication instead of
	 * our session's authentication.
	 */
	if (!strncasecmp(action, "groupdav", 8)) {
		groupdav_main(req, ContentType, /* do GroupDAV methods */
			ContentLength, content+body_start);
		if (!WC->logged_in) {
			WC->killthis = 1;	/* If not logged in, don't */
		}				/* keep the session active */
		goto SKIP_ALL_THIS_CRAP;
	}


	/*
	 * Automatically send requests with any method other than GET or
	 * POST to the GroupDAV code as well.
	 */
	if ((strcasecmp(request_method, "GET")) && (strcasecmp(request_method, "POST"))) {
		groupdav_main(req, ContentType, /* do GroupDAV methods */
			ContentLength, content+body_start);
		if (!WC->logged_in) {
			WC->killthis = 1;	/* If not logged in, don't */
		}				/* keep the session active */
		goto SKIP_ALL_THIS_CRAP;
	}

	/*
	 * If we're not logged in, but we have username and password cookies
	 * supplied by the browser, try using them to log in.
	 */
	if ((!WC->logged_in)
	   && (strlen(c_username) > 0)
	   && (strlen(c_password) > 0)) {
		serv_printf("USER %s", c_username);
		serv_getln(buf, sizeof buf);
		if (buf[0] == '3') {
			serv_printf("PASS %s", c_password);
			serv_getln(buf, sizeof buf);
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
		serv_getln(buf, sizeof buf);
		if (buf[0] == '2') {
			safestrncpy(WC->wc_roomname, c_roomname, sizeof WC->wc_roomname);
		}
	}

	/*
	 * If there are instant messages waiting, retrieve them for display.
	 */
	check_for_instant_messages();

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
	} else if (!strcasecmp(action, "display_aide_menu")) {
		display_aide_menu();
	} else if (!strcasecmp(action, "display_main_menu")) {
		display_main_menu();
	} else if (!strcasecmp(action, "who")) {
		who();
	} else if (!strcasecmp(action, "who_inner_html")) {
		begin_ajax_response();
		who_inner_div();
		end_ajax_response();
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
		if (WC->wc_view != VIEW_MAILBOX) {	/* dotgoto acts like dotskip when we're in a mailbox view */
			slrp_highest();
		}
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
	} else if (!strcasecmp(action, "msg")) {
		embed_message(arg1);
	} else if (!strcasecmp(action, "printmsg")) {
		print_message(arg1);
	} else if (!strcasecmp(action, "display_enter")) {
		display_enter();
	} else if (!strcasecmp(action, "post")) {
		post_message();
	} else if (!strcasecmp(action, "move_msg")) {
		move_msg();
	} else if (!strcasecmp(action, "delete_msg")) {
		delete_msg();
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
	} else if (!strcasecmp(action, "display_whok")) {
		display_whok();
	} else if (!strcasecmp(action, "do_invt_kick")) {
		do_invt_kick();
	} else if (!strcasecmp(action, "display_editroom")) {
		display_editroom();
	} else if (!strcasecmp(action, "netedit")) {
		netedit();
	} else if (!strcasecmp(action, "editroom")) {
		editroom();
	} else if (!strcasecmp(action, "display_editinfo")) {
		display_edit(_("Room info"), "EINF 0", "RINF", "/editinfo", 1);
	} else if (!strcasecmp(action, "editinfo")) {
		save_edit(_("Room info"), "EINF 1", 1);
	} else if (!strcasecmp(action, "display_editbio")) {
		sprintf(buf, "RBIO %s", WC->wc_username);
		display_edit(_("Your bio"), "NOOP", buf, "editbio", 3);
	} else if (!strcasecmp(action, "editbio")) {
		save_edit(_("Your bio"), "EBIO", 0);
	} else if (!strcasecmp(action, "confirm_move_msg")) {
		confirm_move_msg();
	} else if (!strcasecmp(action, "delete_room")) {
		delete_room();
	} else if (!strcasecmp(action, "validate")) {
		validate();
	} else if (!strcasecmp(action, "display_editpic")) {
		display_graphics_upload(_("your photo"),
					"UIMG 0|_userpic_",
					"/editpic");
	} else if (!strcasecmp(action, "editpic")) {
		do_graphics_upload("UIMG 1|_userpic_");
	} else if (!strcasecmp(action, "display_editroompic")) {
		display_graphics_upload(_("the icon for this room"),
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
		display_graphics_upload(_("the icon for this floor"),
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
	} else if (!strcasecmp(action, "siteconfig")) {
		siteconfig();
	} else if (!strcasecmp(action, "display_generic")) {
		display_generic();
	} else if (!strcasecmp(action, "do_generic")) {
		do_generic();
	} else if (!strcasecmp(action, "ajax_servcmd")) {
		ajax_servcmd();
	} else if (!strcasecmp(action, "display_menubar")) {
		display_menubar(1);
	} else if (!strcasecmp(action, "mimepart")) {
		mimepart(arg1, arg2);
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
	} else if (!strcasecmp(action, "summary_inner_div")) {
		begin_ajax_response();
		summary_inner_div();
		end_ajax_response();
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
	} else if (!strcasecmp(action, "setup_wizard")) {
		do_setup_wizard();
	} else if (!strcasecmp(action, "display_preferences")) {
		display_preferences();
	} else if (!strcasecmp(action, "set_preferences")) {
		set_preferences();
	} else if (!strcasecmp(action, "recp_autocomplete")) {
		recp_autocomplete(bstr("recp"));
	} else if (!strcasecmp(action, "cc_autocomplete")) {
		recp_autocomplete(bstr("cc"));
	} else if (!strcasecmp(action, "bcc_autocomplete")) {
		recp_autocomplete(bstr("bcc"));
	} else if (!strcasecmp(action, "diagnostics")) {
		output_headers(1, 1, 1, 0, 0, 0);
		wprintf("Session: %d<hr />\n", WC->wc_session);
		wprintf("Command: <br /><PRE>\n");
		escputs(cmd);
		wprintf("</PRE><hr />\n");
		wprintf("Variables: <br /><PRE>\n");
		dump_vars();
		wprintf("</PRE><hr />\n");
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
